# Common Pitfalls in Telemetry 2.0 Codebase

This document captures recurring anti-patterns and common mistakes found in code reviews for the Telemetry 2.0 embedded framework.

## Memory Management

### Pitfall 1: JSON String Leaks (cJSON)
```c
// ❌ WRONG: cJSON_Print allocates memory
cJSON* json = cJSON_CreateObject();
cJSON_AddStringToObject(json, "key", "value");
char* json_str = cJSON_Print(json);  // Allocates
sendReport(json_str);
cJSON_Delete(json);  // ⚠️ Forgot to free json_str - LEAK!

// ✅ CORRECT:
char* json_str = cJSON_Print(json);
sendReport(json_str);
free(json_str);  // Must free
cJSON_Delete(json);
```

### Pitfall 2: strdup in Profile Parsing
```c
// ❌ WRONG: strdup without checking/freeing
profile->name = strdup(config_name);
profile->protocol = strdup(config_protocol);
// What if strdup fails? What if reassigned?

// ✅ CORRECT:
if (profile->name) free(profile->name);
profile->name = strdup(config_name);
if (!profile->name) {
    return ERR_NO_MEMORY;
}
```

### Pitfall 3: Vector/List Cleanup
```c
// ❌ WRONG: Clearing vector without freeing elements
Vector* markers = profile->markers;
// ... use markers ...
vector_destroy(markers);  // ⚠️ Leaked marker objects inside!

// ✅ CORRECT:
for (int i = 0; i < markers->count; i++) {
    Marker* m = (Marker*)vector_at(markers, i);
    free_marker(m);
}
vector_destroy(markers);
```

## Thread Safety

### Pitfall 4: Profile State Race
```c
// ❌ WRONG: Reading profile state without lock
if (profile->state == PROFILE_ENABLED) {
    collectAndReport(profile);  // Race: state can change
}

// ✅ CORRECT:
pthread_mutex_lock(&profile->mutex);
bool enabled = (profile->state == PROFILE_ENABLED);
pthread_mutex_unlock(&profile->mutex);

if (enabled) {
    collectAndReport(profile);
}
```

### Pitfall 5: rbus Callback Thread Safety
```c
// ❌ WRONG: Modifying global state in rbus callback
rbusError_t eventReceiveHandler(rbusHandle_t handle, rbusEvent_t const* event) {
    g_event_count++;  // Race condition!
    processEvent(event);
    return RBUS_ERROR_SUCCESS;
}

// ✅ CORRECT:
rbusError_t eventReceiveHandler(rbusHandle_t handle, rbusEvent_t const* event) {
    pthread_mutex_lock(&g_event_mutex);
    g_event_count++;
    pthread_mutex_unlock(&g_event_mutex);
    
    processEvent(event);
    return RBUS_ERROR_SUCCESS;
}
```

### Pitfall 6: TimeoutThread vs CollectAndReport Deadlock
```c
// ❌ WRONG: TimeoutThread holds lock while signaling CollectAndReport
void timeoutThread(Profile* profile) {
    pthread_mutex_lock(&profile->mutex);
    // Generate timeout event
    pthread_cond_signal(&profile->event_cond);  
    // CollectAndReport wakes, tries to lock same mutex - DEADLOCK!
    pthread_mutex_unlock(&profile->mutex);
}

// ✅ CORRECT: Signal after releasing lock
void timeoutThread(Profile* profile) {
    pthread_mutex_lock(&profile->mutex);
    profile->timeout_occurred = true;
    pthread_mutex_unlock(&profile->mutex);  // Release first
    pthread_cond_signal(&profile->event_cond);  // Then signal
}
```

## Error Handling

### Pitfall 7: Silent rbus Failures
```c
// ❌ WRONG: Not checking rbus return values
rbusError_t ret = rbus_open(&handle, "T2");
rbusMethod_Register(handle, method);  // Proceeds even if rbus_open failed!

// ✅ CORRECT:
rbusError_t ret = rbus_open(&handle, "T2");
if (ret != RBUS_ERROR_SUCCESS) {
    T2Error("rbus_open failed: %d\n", ret);
    return ERR_RBUS_INIT;
}
```

### Pitfall 8: Ignoring snprintf Return Value
```c
// ❌ WRONG: snprintf truncation not checked
char buffer[64];
snprintf(buffer, sizeof(buffer), "%s_%s_%ld", prefix, name, timestamp);
// What if truncated? Report will be malformed.

// ✅ CORRECT:
char buffer[64];
int written = snprintf(buffer, sizeof(buffer), "%s_%s_%ld", prefix, name, timestamp);
if (written >= sizeof(buffer)) {
    T2Warning("Buffer truncated: needed %d bytes\n", written);
    // Handle truncation
}
```

## Configuration & Schema

### Pitfall 9: Missing Schema Validation
```c
// ❌ WRONG: Trusting JSON without validation
cJSON* interval = cJSON_GetObjectItem(profile_json, "ReportingInterval");
int interval_sec = interval->valueint;  // Crash if NULL or wrong type!

// ✅ CORRECT:
cJSON* interval = cJSON_GetObjectItem(profile_json, "ReportingInterval");
if (!interval || !cJSON_IsNumber(interval)) {
    T2Error("Invalid ReportingInterval in profile\n");
    return ERR_INVALID_CONFIG;
}
int interval_sec = interval->valueint;
if (interval_sec < 60 || interval_sec > 86400) {
    T2Error("ReportingInterval out of range: %d\n", interval_sec);
    return ERR_INVALID_CONFIG;
}
```

### Pitfall 10: Profile Name Collisions
```c
// ❌ WRONG: Not checking for duplicate profiles
void addProfile(Profile* new_profile) {
    vector_push(g_profiles, new_profile);  // Duplicate names possible!
}

// ✅ CORRECT:
bool addProfile(Profile* new_profile) {
    for (int i = 0; i < g_profiles->count; i++) {
        Profile* p = vector_at(g_profiles, i);
        if (strcmp(p->name, new_profile->name) == 0) {
            T2Error("Profile %s already exists\n", new_profile->name);
            return false;
        }
    }
    vector_push(g_profiles, new_profile);
    return true;
}
```

## Scheduler & Timing

### Pitfall 11: Timing Drift in Periodic Reports
```c
// ❌ WRONG: Accumulating drift
void periodicReport(int interval_sec) {
    while (running) {
        sleep(interval_sec);  // Drift accumulates!
        generateReport();
    }
}

// ✅ CORRECT: Absolute timing
void periodicReport(int interval_sec) {
    time_t next_report = time(NULL) + interval_sec;
    while (running) {
        time_t now = time(NULL);
        if (now >= next_report) {
            generateReport();
            next_report = now + interval_sec;  // Reset to now, not next_report
        }
        sleep(1);  // Short sleep to avoid busy-wait
    }
}
```

### Pitfall 12: Timeout Calculation Overflow
```c
// ❌ WRONG: Integer overflow on large intervals
int timeout_ms = interval_sec * 1000;  // Overflow if interval_sec > 2147483!

// ✅ CORRECT:
long timeout_ms = (long)interval_sec * 1000L;
if (timeout_ms > INT_MAX) {
    T2Error("Interval too large: %d seconds\n", interval_sec);
    timeout_ms = INT_MAX;
}
```

## rbus/CCSP Interface

### Pitfall 13: Bus Method Name Typos
```c
// ❌ WRONG: Hardcoded strings prone to typos
rbus_registerMethod(handle, "Device.X_RDKCENTRAL-COM_T2.ReportProfiles");
// vs
rbus_invokeMethod(handle, "Device.X_RDKCENTRAL_COM_T2.ReportProfiles");
// Underscore vs hyphen - won't match!

// ✅ CORRECT: Use constants
#define T2_REPORT_PROFILES_METHOD "Device.X_RDKCENTRAL-COM_T2.ReportProfiles"
rbus_registerMethod(handle, T2_REPORT_PROFILES_METHOD);
rbus_invokeMethod(handle, T2_REPORT_PROFILES_METHOD);
```

### Pitfall 14: Bus Handle Lifetime
```c
// ❌ WRONG: Using handle after close
rbusHandle_t handle;
rbus_open(&handle, "T2");
// ... use handle ...
rbus_close(handle);
rbus_sendData(handle, data);  // ⚠️ Handle invalid!

// ✅ CORRECT: Track handle state
static rbusHandle_t g_handle = NULL;
static bool g_rbus_initialized = false;

void sendData(const char* data) {
    if (!g_rbus_initialized || !g_handle) {
        T2Error("rbus not initialized\n");
        return;
    }
    rbus_sendData(g_handle, data);
}
```

## Data Collection

### Pitfall 15: Missing NULL Checks in TR-181 Fetch
```c
// ❌ WRONG: Assuming parameter always exists
char* value = getParameterValue("Device.DeviceInfo.SerialNumber");
snprintf(report, sizeof(report), "SN:%s", value);  // Crash if NULL!

// ✅ CORRECT:
char* value = getParameterValue("Device.DeviceInfo.SerialNumber");
if (value) {
    snprintf(report, sizeof(report), "SN:%s", value);
    free(value);
} else {
    T2Warning("Failed to get SerialNumber\n");
    snprintf(report, sizeof(report), "SN:UNKNOWN");
}
```

### Pitfall 16: Marker Regex Compilation Every Time
```c
// ❌ WRONG: Recompiling regex on every log line
void processLogLine(const char* line) {
    regex_t regex;
    regcomp(&regex, marker->pattern, REG_EXTENDED);
    if (regexec(&regex, line, 0, NULL, 0) == 0) {
        marker->count++;
    }
    regfree(&regex);  // ⚠️ Expensive!
}

// ✅ CORRECT: Compile once, reuse
typedef struct {
    char* pattern;
    regex_t compiled_regex;
    bool regex_valid;
} Marker;

void initMarker(Marker* m) {
    int ret = regcomp(&m->compiled_regex, m->pattern, REG_EXTENDED);
    m->regex_valid = (ret == 0);
}

void processLogLine(Marker* m, const char* line) {
    if (m->regex_valid && regexec(&m->compiled_regex, line, 0, NULL, 0) == 0) {
        m->count++;
    }
}
```

## Build & Dependencies

### Pitfall 17: Missing Feature Detection in configure.ac
```c
// ❌ WRONG: Assuming rbus is always available
#include <rbus.h>  // Build fails if rbus not installed

// ✅ CORRECT: Configure script checks
AC_CHECK_LIB([rbus], [rbus_open], 
    [have_rbus=yes], 
    [have_rbus=no])
AM_CONDITIONAL([HAVE_RBUS], [test "x$have_rbus" = "xyes"])

// Then in code:
#ifdef HAVE_RBUS
#include <rbus.h>
#else
#include "rbus_stub.h"
#endif
```

### Pitfall 18: Library Link Order
```c
// ❌ WRONG: Random link order in Makefile.am
telemetry2_0_LDADD = -lrbus -lrt -lpthread -lcjson

// ✅ CORRECT: Dependencies before dependents
telemetry2_0_LDADD = -lcjson -lrbus -lrt -lpthread
# cjson has no deps, rbus depends on cjson, pthread is base
```

## Testing

### Pitfall 19: Non-Deterministic Tests
```c
// ❌ WRONG: Test depends on timing
TEST(SchedulerTest, TimeoutFires) {
    startScheduler(1);  // 1 second interval
    sleep(1);
    EXPECT_EQ(1, getReportCount());  // ⚠️ Flaky: may be 0 or 1
}

// ✅ CORRECT: Use mock time or polling
TEST(SchedulerTest, TimeoutFires) {
    startScheduler(1);
    // Poll with timeout
    bool fired = false;
    for (int i = 0; i < 20 && !fired; i++) {
        usleep(100000);  // 100ms
        fired = (getReportCount() > 0);
    }
    EXPECT_TRUE(fired);
}
```

### Pitfall 20: Memory Leaks in Tests
```c
// ❌ WRONG: Setup creates objects, teardown doesn't clean
class ProfileTest : public ::testing::Test {
protected:
    void SetUp() override {
        profile = createProfile("test");
    }
    // ⚠️ Missing TearDown - leaked in every test!
    Profile* profile;
};

// ✅ CORRECT:
class ProfileTest : public ::testing::Test {
protected:
    void SetUp() override {
        profile = createProfile("test");
    }
    void TearDown() override {
        destroyProfile(profile);
        profile = nullptr;
    }
    Profile* profile;
};
```

## Platform Portability

### Pitfall 21: Hardcoded Paths
```c
// ❌ WRONG: Linux-specific path
#define CONFIG_FILE "/etc/telemetry/T2Agent.cfg"

// ✅ CORRECT: Configurable path
#ifndef T2_CONFIG_DIR
#define T2_CONFIG_DIR "/etc/telemetry"
#endif
#define CONFIG_FILE T2_CONFIG_DIR "/T2Agent.cfg"
```

### Pitfall 22: Endianness Assumptions
```c
// ❌ WRONG: Assuming little-endian
uint32_t net_value = *(uint32_t*)buffer;  // Wrong on big-endian!

// ✅ CORRECT: Use network byte order functions
uint32_t net_value = ntohl(*(uint32_t*)buffer);
```

## Review Checklist: Avoid These Pitfalls

When reviewing PRs, specifically check for:
- [ ] cJSON memory management (free cJSON_Print results)
- [ ] Profile state access (always use mutex)
- [ ] rbus return value checking
- [ ] Schema validation before using JSON values
- [ ] Profile name uniqueness enforcement
- [ ] Timing calculations (avoid drift and overflow)
- [ ] Bus method name consistency (use constants)
- [ ] TR-181 parameter NULL checks
- [ ] Regex compilation (do it once, not per line)
- [ ] Feature detection in build system
- [ ] Test determinism (no timing dependencies)
- [ ] Test cleanup (TearDown matches SetUp)
- [ ] Path portability (no hardcoded Linux paths)
