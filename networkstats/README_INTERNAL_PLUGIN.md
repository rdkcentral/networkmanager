# NetworkConnectionStats - Internal Thunder Plugin (No External APIs)

## Design Philosophy

This plugin is designed to run **internally within Thunder framework** without exposing any external APIs. It automatically performs network diagnostics and reports telemetry data in the background.

## Architecture

```
┌─────────────────────────────────────────┐
│    Thunder/WPEFramework Process         │
│  ┌───────────────────────────────────┐  │
│  │  NetworkConnectionStats           │  │
│  │  (Plugin - IPlugin only)          │  │
│  │  - Auto-activates                 │  │
│  │  - No JSONRPC APIs                │  │
│  │  - Spawns out-of-process          │  │
│  └──────────┬────────────────────────┘  │
└─────────────┼───────────────────────────┘
              │ COM-RPC (internal)
              ▼
┌─────────────────────────────────────────┐
│   Out-of-Process (Background Service)   │
│  ┌───────────────────────────────────┐  │
│  │ NetworkConnectionStatsImpl        │  │
│  │ - Auto-starts periodic reporting  │  │
│  │ - Runs every N minutes (default10)│  │
│  │ - Generates diagnostic reports    │  │
│  │ - Sends T2 telemetry events       │  │
│  │ - No external APIs exposed        │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## What This Plugin Does

### On Activation
1. Thunder activates the plugin automatically (`startmode: "Activated"`)
2. Plugin spawns out-of-process implementation
3. Implementation initializes network provider
4. **Generates initial diagnostic report**
5. **Starts periodic reporting thread** (runs every N minutes)

### Periodic Diagnostics (Every N Minutes)
The plugin automatically runs these checks:

1. **Connection Type Check**
   - Detects Ethernet vs WiFi
   - Logs to telemetry: `Connection_Type`

2. **IP Configuration Check**
   - Gets interface name, IPv4/IPv6 addresses
   - Gets IPv4/IPv6 gateways and DNS servers
   - Logs to telemetry: `Network_Interface_Info`, `IPv4_Gateway_DNS`, `IPv6_Gateway_DNS`

3. **IPv4 Route Check**
   - Validates IPv4 gateway availability
   - Traces warnings if no gateway found

4. **IPv6 Route Check**
   - Validates IPv6 gateway availability
   - Traces warnings if no gateway found

5. **Gateway Packet Loss Check**
   - Pings IPv4 gateway (5 packets, 30s timeout)
   - Pings IPv6 gateway (5 packets, 30s timeout)
   - Measures packet loss percentage and average RTT
   - Logs to telemetry: `Gateway_Ping_Stats`

6. **DNS Configuration Check**
   - Verifies DNS servers are configured
   - Logs warnings if no DNS found

### Telemetry Events Sent

All events use the T2 telemetry framework:

| Event Name | Data Format | Example |
|------------|-------------|---------|
| `Connection_Type` | String | `"Ethernet"` or `"WiFi"` |
| `Network_Interface_Info` | CSV | `"eth0,192.168.1.100,fe80::1"` |
| `IPv4_Gateway_DNS` | CSV | `"192.168.1.1,8.8.8.8"` |
| `IPv6_Gateway_DNS` | CSV | `"fe80::1,2001:4860:4860::8888"` |
| `Gateway_Ping_Stats` | CSV | `"IPv4,192.168.1.1,0.0,2.5"` (version,gateway,loss%,avgRTT) |
| `DNS_Status` | String | `"No DNS configured"` (only on error) |

## Configuration

### NetworkConnectionStats.config
```json
{
  "locator": "libWPEFrameworkNetworkConnectionStats.so",
  "classname": "NetworkConnectionStats",
  "startmode": "Activated",
  "configuration": {
    "outofprocess": true,
    "root": { "mode": "Local" },
    "reportingInterval": 10,
    "autoStart": true
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `reportingInterval` | number | 10 | Diagnostic interval in minutes |
| `autoStart` | boolean | true | Auto-start periodic reporting on plugin activation |
| `outofprocess` | boolean | true | Run implementation out-of-process (recommended) |

## Files Structure

### Core Plugin Files
- **Module.h/cpp** - Thunder module definition
- **INetworkConnectionStats.h** - Minimal COM-RPC interface (no methods)
- **NetworkConnectionStats.h** - Plugin class (IPlugin only)
- **NetworkConnectionStats.cpp** - Plugin implementation
- **NetworkConnectionStatsImplementation.h** - Out-of-process implementation header
- **NetworkConnectionStatsImplementation.cpp** - All diagnostic logic

### Supporting Files
- **INetworkData.h** - Network data provider interface
- **ThunderJsonRPCProvider.h/cpp** - NetworkManager plugin client
- **NetworkConnectionStats.config** - Plugin configuration
- **NetworkConnectionStats.conf.in** - CMake config template
- **CMakeLists.txt** - Build system

## No External APIs

### What Was Removed
- ❌ JSON-RPC API methods
- ❌ External event notifications
- ❌ Public getter/setter methods
- ❌ NetworkConnectionStats.json API specification
- ❌ INotification interface

### What Remains
- ✅ IPlugin interface (Initialize, Deinitialize, Information)
- ✅ IConfiguration interface (for plugin config)
- ✅ Internal diagnostic methods
- ✅ Telemetry logging
- ✅ Periodic reporting thread

## How It Works

### Startup Sequence
```
1. Thunder starts
2. Thunder loads NetworkConnectionStats plugin
3. Plugin Initialize() called
4. Plugin spawns out-of-process NetworkConnectionStatsImplementation
5. Implementation Configure() called
6. NetworkJsonRPCProvider initialized
7. Initial diagnostic report generated immediately
8. Periodic reporting thread started (if autoStart=true)
9. Thread sleeps for 'reportingInterval' minutes
10. Thread wakes up, generates report
11. Repeat steps 9-10 until plugin deactivated
```

### Logging

All diagnostic output goes to Thunder trace logs:
```bash
# View logs
journalctl -u wpeframework -f | grep NetworkConnectionStats

# Example output:
INFO: NetworkConnectionStats Constructor
INFO: NetworkConnectionStatsImplementation::Configure
INFO: Periodic reporting started with 10 minute interval
INFO: Connection type: Ethernet
INFO: Interface: eth0, IPv4: 192.168.1.100, IPv6: fe80::1
INFO: IPv4 Gateway: 192.168.1.1, DNS: 8.8.8.8
INFO: Pinging IPv4 gateway: 192.168.1.1
INFO: IPv4 gateway ping - Loss: 0.0%, RTT: 2.5ms
INFO: Network diagnostics report completed
INFO: Periodic report generated
```

## Building

```bash
cmake -B build \
  -DPLUGIN_NETWORKCONNECTIONSTATS=ON \
  -DPLUGIN_NETWORKCONNECTIONSTATS_OUTOFPROCESS=ON \
  -DUSE_TELEMETRY=ON \
  -DPLUGIN_NETWORKCONNECTIONSTATS_REPORTING_INTERVAL=10

cmake --build build
sudo cmake --install build
```

## Deployment

### Install Plugin
```bash
# Libraries installed to:
/usr/lib/wpeframework/plugins/libWPEFrameworkNetworkConnectionStats.so
/usr/lib/wpeframework/plugins/libWPEFrameworkNetworkConnectionStatsImplementation.so

# Config installed to:
/etc/wpeframework/plugins/NetworkConnectionStats.json
```

### Thunder Auto-Activation
Plugin automatically activates on Thunder startup (no manual activation needed).

## Monitoring

### Check Plugin Status
```bash
curl http://localhost:9998/jsonrpc -d '{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "Controller.1.status@NetworkConnectionStats"
}'
```

### Expected Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "callsign": "NetworkConnectionStats",
      "locator": "libWPEFrameworkNetworkConnectionStats.so",
      "classname": "NetworkConnectionStats",
      "autostart": true,
      "state": "activated"
    }
  ]
}
```

### View Telemetry Events
Telemetry events are sent via T2 framework and uploaded to backend systems. Check T2 logs:
```bash
journalctl -u t2 -f
```

## Advantages of Internal-Only Design

1. **Simplicity** - No API surface to maintain or secure
2. **Background Operation** - Runs autonomously without external triggers
3. **Zero Configuration** - Works out-of-the-box with defaults
4. **Resource Efficient** - Periodic checks minimize CPU/network usage
5. **Telemetry Integration** - Data flows automatically to analytics backend
6. **Process Isolation** - Out-of-process prevents Thunder crashes
7. **Automatic Recovery** - Thunder restarts implementation if it crashes

## Use Cases

This plugin is ideal for:
- **Continuous network monitoring** - Track connection health 24/7
- **Proactive diagnostics** - Detect issues before users report them
- **Analytics/BigData** - Feed network metrics to data pipelines
- **Fleet management** - Monitor thousands of devices remotely
- **SLA tracking** - Measure uptime and connectivity quality

## Comparison: Standalone vs Thunder Plugin

| Aspect | Standalone (Old) | Thunder Plugin (New) |
|--------|------------------|----------------------|
| **Execution** | Separate process with main() | Thunder-managed lifecycle |
| **Startup** | Manual launch | Auto-activated by Thunder |
| **APIs** | None | None (internal-only) |
| **Logging** | printf/fprintf | Thunder TRACE framework |
| **Telemetry** | T2 direct calls | T2 direct calls (same) |
| **Configuration** | Hardcoded | Thunder config system |
| **Monitoring** | None | Thunder Controller API |
| **Integration** | Isolated | Part of Thunder ecosystem |

## Summary

NetworkConnectionStats is now a **self-contained Thunder plugin** that:
- ✅ Runs automatically in the background
- ✅ Performs periodic network diagnostics (every N minutes)
- ✅ Sends telemetry data to T2 framework
- ✅ Requires zero external interaction
- ✅ Has no exposed APIs (internal-only)
- ✅ Integrates seamlessly with Thunder framework
- ✅ Provides continuous network health monitoring

**Perfect for production deployments that need passive network monitoring without exposing attack surfaces!**
