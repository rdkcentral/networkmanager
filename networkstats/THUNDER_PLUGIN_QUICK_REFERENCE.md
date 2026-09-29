# NetworkConnectionStats Thunder Plugin - Quick Reference

## Architecture Pattern (Following Telemetry)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Thunder/WPEFramework Process                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │         NetworkConnectionStats (Plugin)                     │ │
│  │  - Inherits IPlugin + JSONRPC                              │ │
│  │  - Manages RPC connection                                  │ │
│  │  - Forwards JSON-RPC calls                                 │ │
│  │  - Handles notifications                                   │ │
│  └────────────────┬───────────────────────────────────────────┘ │
│                   │ COM-RPC                                      │
└───────────────────┼──────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────┐
│            Out-of-Process (Separate Process)                     │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │    NetworkConnectionStatsImplementation                     │ │
│  │  - Implements INetworkConnectionStats                      │ │
│  │  - Implements IConfiguration                               │ │
│  │  - Contains business logic                                 │ │
│  │  - Manages network diagnostics                             │ │
│  │  - Sends notifications via INotification                   │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  NetworkJsonRPCProvider (INetworkData)               │  │ │
│  │  │  - Calls NetworkManager plugin APIs                  │  │ │
│  │  │  - Gets network interface info                       │  │ │
│  │  │  - Performs ping tests                               │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## File Mapping: Telemetry → NetworkConnectionStats

| Telemetry | NetworkConnectionStats | Purpose |
|-----------|------------------------|---------|
| Module.h/cpp | Module.h/cpp | Thunder module definition |
| ITelemetry.h | INetworkConnectionStats.h | COM-RPC interface |
| Telemetry.h | NetworkConnectionStats.h | Plugin class header |
| Telemetry.cpp | NetworkConnectionStats.cpp | Plugin implementation |
| TelemetryImplementation.h | NetworkConnectionStatsImplementation.h | Out-of-process header |
| TelemetryImplementation.cpp | NetworkConnectionStatsImplementation.cpp | Out-of-process impl |
| Telemetry.json | NetworkConnectionStats.json | JSON-RPC API spec |
| Telemetry.config | NetworkConnectionStats.config | Config file |

## Key Classes & Interfaces

### 1. NetworkConnectionStats (Plugin)
**File**: NetworkConnectionStats.h/cpp  
**Inherits**: PluginHost::IPlugin, PluginHost::JSONRPC  
**Purpose**: Main plugin class running in Thunder process

**Key Methods**:
- `Initialize(PluginHost::IShell*)` - Plugin activation
- `Deinitialize(PluginHost::IShell*)` - Plugin cleanup
- `Information()` - Plugin description
- `Deactivated(RPC::IRemoteConnection*)` - Handle out-of-process crash

**Inner Class**: `Notification`
- Implements `RPC::IRemoteConnection::INotification`
- Implements `INetworkConnectionStats::INotification`
- Forwards events to JSON-RPC clients

### 2. INetworkConnectionStats (Interface)
**File**: INetworkConnectionStats.h  
**Purpose**: COM-RPC interface definition

**Sub-Interface**: `INotification`
- `OnReportGenerated(const string&)` - Event notification

**Main Interface Methods**:
- `Register/Unregister` - Notification subscription
- `GenerateReport()` - Trigger diagnostics
- `GetConnectionType()` - Network type
- `GetIpv4Address/GetIpv6Address` - IP addresses
- `GetIpv4Gateway/GetIpv6Gateway` - Gateways
- `GetIpv4Dns/GetIpv6Dns` - DNS servers
- `GetInterface()` - Active interface
- `PingGateway()` - Gateway connectivity test
- `SetPeriodicReporting()` - Enable/disable periodic reports
- `SetReportingInterval()` - Configure interval

### 3. NetworkConnectionStatsImplementation
**File**: NetworkConnectionStatsImplementation.h/cpp  
**Inherits**: INetworkConnectionStats, IConfiguration  
**Purpose**: Out-of-process business logic

**Key Methods**:
- `Configure(PluginHost::IShell*)` - Initialize from config
- All INetworkConnectionStats interface methods
- `connectionTypeCheck()` - Internal diagnostic
- `connectionIpCheck()` - Internal diagnostic
- `defaultIpv4RouteCheck()` - Internal diagnostic
- `defaultIpv6RouteCheck()` - Internal diagnostic
- `gatewayPacketLossCheck()` - Internal diagnostic
- `networkDnsCheck()` - Internal diagnostic
- `logTelemetry()` - T2 event logging
- `periodicReportingThread()` - Background thread

## Interface Macros Explained

### BEGIN_INTERFACE_MAP / END_INTERFACE_MAP
Thunder's interface query mechanism (like COM's QueryInterface):

```cpp
BEGIN_INTERFACE_MAP(NetworkConnectionStats)
    INTERFACE_ENTRY(PluginHost::IPlugin)           // Standard plugin interface
    INTERFACE_ENTRY(PluginHost::IDispatcher)       // JSON-RPC dispatcher
    INTERFACE_AGGREGATE(Exchange::INetworkConnectionStats, _networkStats)  // Delegate to out-of-process
END_INTERFACE_MAP
```

**INTERFACE_AGGREGATE**: Forwards interface queries to another object (_networkStats = out-of-process impl)

## RPC Connection Flow

1. **Plugin Activation** (In-process):
   ```cpp
   Initialize(service) {
       _networkStats = service->Root<INetworkConnectionStats>(_connectionId, 5000, "NetworkConnectionStatsImplementation");
       // Spawns out-of-process, creates proxy
   }
   ```

2. **Out-of-process Creation**:
   - Thunder spawns new process
   - Loads NetworkConnectionStatsImplementation library
   - Creates instance via SERVICE_REGISTRATION macro
   - Establishes RPC channel

3. **API Call Flow**:
   ```
   JSON-RPC Client → Thunder → NetworkConnectionStats (plugin) 
   → COM-RPC Proxy → Out-of-process → NetworkConnectionStatsImplementation
   → NetworkJsonRPCProvider → NetworkManager Plugin
   ```

4. **Event Flow**:
   ```
   NetworkConnectionStatsImplementation::notifyReportGenerated()
   → INotification::OnReportGenerated()
   → COM-RPC channel
   → NetworkConnectionStats::Notification::OnReportGenerated()
   → JNetworkConnectionStats::Event::OnReportGenerated()
   → JSON-RPC Event to subscribed clients
   ```

## Critical Thunder Patterns

### 1. SERVICE_REGISTRATION
Registers class with Thunder's factory:
```cpp
SERVICE_REGISTRATION(NetworkConnectionStats, 1, 0, 0);
```

### 2. Metadata Declaration
Plugin version and lifecycle info:
```cpp
static Plugin::Metadata<Plugin::NetworkConnectionStats> metadata(
    1, 0, 0,  // Major, Minor, Patch
    {},       // Preconditions
    {},       // Terminations
    {}        // Controls
);
```

### 3. Notification Pattern
```cpp
// Plugin side
Core::Sink<Notification> _notification;

// Implementation side
std::list<INetworkConnectionStats::INotification*> _notifications;

Register() { 
    _notifications.push_back(notification);
    notification->AddRef();  // Reference counting!
}

Unregister() {
    notification->Release();
    _notifications.erase(notification);
}
```

### 4. Thread Safety
```cpp
Core::CriticalSection _adminLock;

void SomeMethod() {
    _adminLock.Lock();
    // Critical section
    _adminLock.Unlock();
}
```

### 5. Reference Counting
```cpp
_service->AddRef();   // Increment ref count
_service->Release();  // Decrement ref count
// Object destroyed when ref count reaches 0
```

## Configuration File Format

### NetworkConnectionStats.config
```json
{
  "locator": "libWPEFrameworkNetworkConnectionStats.so",  // Plugin library
  "classname": "NetworkConnectionStats",                   // Class name
  "startmode": "Activated",                               // Auto-start
  "configuration": {
    "outofprocess": true,                                 // Run out-of-process
    "root": {
      "mode": "Local"                                     // Process mode
    },
    "reportingInterval": 10                               // Custom config
  }
}
```

## Build Integration

### Add to Parent CMakeLists.txt
```cmake
add_subdirectory(networkstats)
```

### Build Commands
```bash
cd build
cmake .. -DPLUGIN_NETWORKCONNECTIONSTATS=ON -DUSE_TELEMETRY=ON
make NetworkConnectionStats
sudo make install
```

## Debugging Tips

### 1. Enable Traces
Set log level in Thunder config or via Controller plugin:
```bash
curl http://localhost:9998/jsonrpc -d '{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "Controller.1.configuration@NetworkConnectionStats",
  "params": {"trace": ["Information", "Error"]}
}'
```

### 2. Check Process
```bash
ps aux | grep NetworkConnectionStatsImplementation
```

### 3. Monitor Logs
```bash
journalctl -u wpeframework -f | grep NetworkConnectionStats
```

### 4. Verify Plugin Load
```bash
curl http://localhost:9998/jsonrpc -d '{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "Controller.1.status@NetworkConnectionStats"
}'
```

## Common Issues & Solutions

### Issue: Plugin doesn't load
**Check**: 
- Library in correct path (`/usr/lib/wpeframework/plugins/`)
- Dependencies present (`ldd libWPEFrameworkNetworkConnectionStats.so`)
- Config file syntax valid

### Issue: Out-of-process crashes
**Check**:
- NetworkJsonRPCProvider can reach NetworkManager plugin
- Null pointer checks in implementation
- Thread synchronization

### Issue: Events not received
**Check**:
- Client subscribed to event
- Notification registered in implementation
- RPC connection active

### Issue: "Interface not found"
**Check**:
- INTERFACE_AGGREGATE correctly set
- Out-of-process implementation exports interface
- SERVICE_REGISTRATION present

## Testing Checklist

- [ ] Plugin activates successfully
- [ ] All JSON-RPC methods callable
- [ ] Events received by subscribers
- [ ] Periodic reporting works
- [ ] Out-of-process survives restarts
- [ ] Telemetry events logged (if enabled)
- [ ] No memory leaks (valgrind)
- [ ] Thread-safe under concurrent calls
- [ ] Graceful deactivation/cleanup

## Resources

- **Thunder Documentation**: https://rdkcentral.github.io/Thunder/
- **Plugin Template**: Telemetry plugin (reference implementation)
- **API Generator**: JsonGenerator.py in Thunder tools
- **Code Examples**: Other Thunder plugins in RDK repositories
