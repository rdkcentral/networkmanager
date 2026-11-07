---
name: triage-logs
description: >
  Triage any Telemetry 2.0 behavioral issue on RDK devices by correlating device
  log bundles with source code. Covers hangs, under-reporting, over-reporting,
  duplicate reports, CPU/memory spikes, scheduler anomalies, rbus problems, and
  test gap analysis. The user states the issue; this skill guides systematic
  root-cause analysis regardless of issue type.
---

# Telemetry 2.0 Issue Triage Skill

## Purpose

Systematically correlate device log bundles with Telemetry 2.0 source code to
identify root causes, characterize impact, and propose unit-test and
functional-test reproduction scenarios — for **any** behavioral anomaly reported
by the user.

---

## Usage

Invoke this skill when:
- A device log bundle is available under `logs/` (or attached separately)
- The user describes a behavioral anomaly (examples: daemon stuck, reports
  missing, too many reports sent, reports arriving late, high CPU, high memory,
  unexpected profile activation, marker counts wrong)
- You need to write a reproduction scenario for an existing or proposed fix

**The user's stated issue drives the investigation.** Do not assume a specific
failure mode — read the issue description first, then follow the steps below.

---

## Step 1: Orient to the Log Bundle

**Log bundle layout** (typical RDK device):
```
logs/<MAC>/<SESSION_TIMESTAMP>/logs/
    telemetry2_0.txt.0        ← Primary T2 daemon log (start here)
    GatewayManagerLog.txt.0   ← WAN/gateway state machine
    WanManager*.txt.0         ← WAN interface transitions
    PAMlog.txt.0              ← Platform/parameter management
    SelfHeal*.txt.0           ← Watchdog and recovery events
    top_log.txt.0             ← CPU/memory snapshots (useful for perf issues)
    messages.txt.0            ← Kernel and system messages
```

Include any log files surfaced by the user's issue description (e.g., `cellular*.txt.0`
for connectivity issues, `syslog` for OOM events).

**Log timestamp prefix format**: `YYMMDD-HH:MM:SS.uuuuuu`
- Session folder names are **local-time snapshots** (format: `MM-DD-YY-HH:MMxM`)
- Log lines inside use device local time — always confirm via `[Time]` field
  in telemetry reports (`"Time":"2026-03-06 07:24:23"`)
- Report JSON `"timestamp"` fields are Unix epoch UTC

**Session ordering**: Sort session folders chronologically. Multiple sessions may
represent reboots. Alphabetical sort does NOT equal chronological order.

---

## Step 2: Map Profiles and Threads

Read the startup section of `telemetry2_0.txt.0` (first ~50 lines) to identify:

| What to find | Log pattern |
|---|---|
| Profile name | `Profile Name : <name>` |
| Reporting interval | `Waiting for <N> sec for next TIMEOUT` |
| Timeout thread TID | `TIMEOUT for profile - <name>` (first occurrence) |
| CollectAndReport TID | `CollectAndReport ++in profileName : <name>` (first occurrence) |
| Send mechanism | `methodName = Device.X_RDK_Xmidt.SendData` (rbus) or `HTTP_CODE` (curl) |

**Thread role map** (look for TID in `TimeoutThread` context):
- `TimeoutThread` per profile — fires `TIMEOUT for profile` log lines
- `CollectAndReport` / `CollectAndReportXconf` — one per profile, generates/sends reports
- `asyncMethodHandler` — short-lived rbus handler thread, called when `SendData` is dispatched

---

## Step 3: Identify the Anomaly Window

Based on the **user's stated issue**, search for the relevant evidence pattern:

### Hang / Stuck Daemon
A reporting hang manifests as a **timestamp gap** between `CollectAndReport ++in` and
the next report-related log line from the same TID.
```
grep -n "CollectAndReport" telemetry2_0.txt.0 | head -40
```
Gap > 1 reporting interval = anomaly. During the gap, check:
- Is `asyncMethodHandler` ever logged? (no → rbus provider unresponsive)
- Does `TIMEOUT for profile` still fire? (yes → TimeoutThread alive but CollectAndReport stuck)

### Under-Reporting / Missing Reports
Look for expected `TIMEOUT for profile` events that never trigger a `CollectAndReport`:
```
grep -n "TIMEOUT for profile\|CollectAndReport ++in\|Return status" telemetry2_0.txt.0
```
- Count `TIMEOUT` events vs. `CollectAndReport` entries over a time window
- Check for `SendInterruptToTimeoutThread` logged as failed (EBUSY path) — signals silently dropped
- Check for profile deactivation or reload during expected report window

### Over-Reporting / Duplicate Reports
Look for multiple `CollectAndReport ++in` within a single interval:
```
grep -n "CollectAndReport ++in\|TIMEOUT for profile" telemetry2_0.txt.0
```
- Multiple `TIMEOUT` signals in one interval → concurrent interrupt and scheduler fire
- Report-on-condition (`T2ERROR_SUCCESS` after a marker event) firing alongside periodic report
- Check `signalrecived_and_executing` global flag race (concurrent profile callbacks)

### CPU / Memory Spikes
Correlate `top_log.txt.0` timestamps with T2 activity:
```
grep -n "telemetry2" top_log.txt.0
```
- Identify what T2 was doing (profile scan, DCA grep, report generation, rbus call) at spike time
- Check DCA log grep operations (`dca.c`, `dcautil.c`) for large log files causing high CPU
- Check marker accumulation in `t2markers.c` for memory growth
- Check if multiple profiles overlap their `CollectAndReport` window

### Profile / Configuration Anomalies
- Unexpected profile changes: `grep -n "profile\|xconf" telemetry2_0.txt.0 | grep -i "receiv\|updat\|activ"`
- Marker count mismatches: compare report JSON marker values against grep patterns in `dca.c`
- Wrong reporting interval: confirm `Waiting for <N> sec` matches profile definition

---

## Step 4: Correlate with Other Component Logs

Based on the anomaly window identified in Step 3, cross-reference with other logs:

| Issue Type | Companion Log | What to Look For |
|---|---|---|
| Hang / rbus block | `GatewayManagerLog.txt.0` | WAN/interface state changes within hang window |
| Hang / rbus block | `WanManager*.txt.0` | Interface up/down transitions |
| Under-reporting | `SelfHeal*.txt.0` | Watchdog restarts of telemetry2_0 process |
| Over-reporting | `PAMlog.txt.0` | Parameter changes triggering report-on-condition |
| CPU spike | `top_log.txt.0` | CPU% at anomaly timestamps |
| Memory growth | `messages.txt.0` | OOM killer events, slab usage |
| Profile changes | Any xconf response log | Profile push or xconf poll activity |

A tight coupling between an external event (state change, parameter update, restart)
and the T2 anomaly window is the primary indicator of cause vs. coincidence.

---

## Step 5: Locate the Code Path

Navigate to the relevant source based on the anomaly type. Key modules:

### Scheduler (`source/scheduler/scheduler.c`)

Controls when profiles fire. Key paths:
- **`TimeoutThread`** — per-profile thread; calls `timeoutNotificationCb` while holding `tMutex`
- **`SendInterruptToTimeoutThread`** — uses `pthread_mutex_trylock`; if `tMutex` is held
  (callback in progress), the interrupt is **silently dropped** (EBUSY returns `T2ERROR_FAILURE`)
- **`signalrecived_and_executing`** — global flag with no atomic protection; susceptible
  to concurrent-write races under multi-profile load

### Profile / Report Generation (`source/bulkdata/profile.c`, `profilexconf.c`, `reportprofiles.c`)

- `CollectAndReport` / `CollectAndReportXconf` hold `plMutex` or `reuseThreadMutex`
  for the entire report lifecycle (collection + send)
- **rbus send** (`rbusMethod_Invoke` / `rbusMethod_InvokeAsync`) has **no timeout** —
  a blocked rbus provider blocks the entire thread indefinitely
- Report-on-condition logic in `reportprofiles.c` can fire concurrently with a
  periodic send if synchronization is missing

### Data Collection / CPU (`source/dcautil/dca.c`, `dcautil.c`, `dcacpu.c`, `dcamem.c`)

- DCA log-grep is I/O and CPU intensive; large log files can cause CPU spikes
- `dcacpu.c` and `dcamem.c` sample system resources; misreads can cause false markers
- Marker accumulation without cleanup (`t2markers.c`) can grow heap over time

### Profile Configuration (`source/t2parser/`, `source/bulkdata/profilexconf.c`)

- Profile reception, parsing, and activation path for xconf-sourced profiles
- Incorrect interval parsing or duplicate profile names can cause
  over-scheduling or silent deactivation

### Transport Layer (`source/protocol/http/`, `source/protocol/rbusMethod/`)

- HTTP send failures, retry logic, and cached-report replay
- rbus method provider registration and response handling

---

## Step 6: Characterize Root Cause

Use this matrix to classify the issue based on observed evidence:

| Observed Pattern | Issue Class | Primary Code Location |
|---|---|---|
| rbus call blocks > 10s, no `asyncMethodHandler` logged | Rbus provider unresponsive | `profile.c` / rbus transport |
| `Signal Thread To restart` logged but no report follows | Interrupt signal dropped (EBUSY on `tMutex`) | `scheduler.c:SendInterruptToTimeoutThread` |
| `TIMEOUT` fires but `CollectAndReport` never starts | Thread pool exhausted or profile in error state | `scheduler.c`, `reportprofiles.c` |
| `TIMEOUT` entries missing for > 2 intervals | TimeoutThread stuck, exited, or profile deregistered | `scheduler.c:TimeoutThread` |
| Multiple `CollectAndReport ++in` within one interval | Over-scheduling: concurrent interrupt + periodic fire | `scheduler.c`, `reportprofiles.c` |
| Long gap between `++in` and `--out` with HTTP errors | Network failure; cached report retry loop | `profilexconf.c`, HTTP transport |
| Report JSON marker counts lower than expected | DCA grep miss, log rotation during scan, or marker not registered | `dca.c`, `t2markers.c` |
| Report JSON marker counts higher than expected | Duplicate marker registration or over-counting in DCA | `dca.c`, `t2markers.c` |
| `signalrecived_and_executing` logic inconsistency | Unsynchronized global flag race | `scheduler.c` (global variable) |
| CPU spike during report window | Large log file DCA grep or concurrent profile collection | `dcautil.c`, `dca.c` |
| Memory growth over sessions | Marker list not freed, profile not cleaned up on deregister | `t2markers.c`, `profile.c` |
| Profile activated/deactivated unexpectedly | xconf push race or profile name collision | `profilexconf.c`, `t2parser/` |

---

## Step 7: Assess L1 (Unit) Test Coverage

**Location**: `source/test/`

**Existing coverage** (representative):
- `schedulerTest.cpp`: basic `SendInterruptToTimeoutThread`, `TimeoutThread` single-run,
  profile register/unregister lifecycle
- `profileTest.cpp`: profile creation, marker accumulation, basic report generation
- `dcaTest.cpp`: grep pattern matching, marker extraction

**Identify gaps relevant to the issue**. For each gap, write a test template:

```
Test Name: <what it verifies>
Setup:     <initial conditions>
Action:    <what triggers the behavior>
Assert:    <what correct behavior looks like>
File:      source/test/<module>/
```

**Common gap areas** (match to the issue class):
- Scheduler signal dropped when `tMutex` held during callback (EBUSY path)
- `CollectAndReport` blocked while scheduler fires multiple subsequent timeouts
- DCA grep on a large/rotating log file — correct marker counts
- Profile re-activation during an active `CollectAndReport` — no double-send
- Memory freed correctly when profile is deregistered mid-cycle
- `signalrecived_and_executing` flag read/write under concurrent profile load

---

## Step 8: Assess L2 (Functional) Test Coverage

**Location**: `test/functional-tests/features/`

**Existing scenarios** (from `.feature` files):
- `telemetry_process_singleprofile.feature` — caching on send failure
- `telemetry_process_multiprofile.feature` — multi-profile interaction
- `telemetry_bootup_sequence.feature`, `telemetry_runs_as_daemon.feature`
- `telemetry_process_tempProfile.feature`, `telemetry_xconf_communication.feature`

**Identify the missing scenario** that would catch the reported issue. Write a
Gherkin outline covering:
1. The precondition (profile active, network state, system load)
2. The triggering event (external state change, concurrent interrupt, large log file, etc.)
3. The correct observable outcome (report sent within interval, no duplicate, CPU within bounds)
4. The failure observable outcome (what the bug produces vs. what is expected)

```gherkin
Feature: <Short description of the behavioral area>

  Scenario: <Specific failure scenario title>
    Given <system precondition>
    And   <additional setup>
    When  <triggering event>
    Then  <expected correct behavior>
    And   <no regression assertion>
```

---

## Step 9: Document Findings

Produce a triage report with:
1. **Issue restatement**: confirm back the user's stated problem in one sentence
2. **Device context**: MAC, firmware, session timestamp(s) examined
3. **Anomaly timeline**: exact timestamps, thread IDs, duration or frequency
4. **Root cause chain**: numbered steps, each with log evidence + source code reference
5. **L1 test gap**: which test file, test name, and what assertion it makes
6. **L2 test gap**: Gherkin scenario outline
7. **Proposed fix**: minimum-scope change — file, function, and what to change

---

## Common Pitfalls

- **Timestamp confusion**: Log header `260306-HH:MM:SS` = `2026-03-06`; report JSON
  `"timestamp":"177xxxxxxx.xx"` is Unix epoch UTC — do not mix them
- **Session folder order**: Alphabetical sort does NOT equal chronological order
- **`signalrecived_and_executing`**: This global has a typo in the source ("recived") —
  search for it exactly as spelled
- **EBUSY ≠ deadlock**: The `trylock` in `SendInterruptToTimeoutThread` prevents
  deadlock but causes **silent signal loss** — the thread is not stuck, the interrupt
  was simply never delivered
- **`asyncMethodHandler` absence**: No log of this thread during an rbus call means
  the rbus provider never received the request — distinguish from a network-only issue
- **Double-log artifact**: T2 logs `methodName = ...` twice per send in some builds —
  this is a logging artifact, not two actual sends; verify by counting `Return status` lines
- **Profile count vs. report count**: A profile may be registered but never reach
  `CollectAndReport` if upstream conditions are not met — trace from `TIMEOUT` forward
- **DCA grep on rotated logs**: If a log file rotates mid-scan, DCA may return 0 for
  a marker that was incremented — correlates to under-reporting without any error log
