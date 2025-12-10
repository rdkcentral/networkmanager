# ThunderJsonRPCProvider Test Application

## Overview
This test application validates the NetworkJsonRPCProvider class which communicates with the NetworkManager Thunder plugin via JSON-RPC over HTTP.

## Prerequisites

### Required Libraries
- **jsoncpp**: JSON parsing library
- **libjsonrpccpp**: JSON-RPC C++ framework (client and common)

### Installation

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libjsonrpccpp-dev libjsoncpp-dev
```

#### Fedora/RHEL
```bash
sudo dnf install jsoncpp-devel libjsonrpccpp-devel
```

#### macOS (Homebrew)
```bash
brew install jsoncpp libjson-rpc-cpp
```

## Building

### Option 1: Using the build script (Recommended)
```bash
cd networkstats
./build.sh
```

### Option 2: Using Makefile
```bash
cd networkstats
make
```

### Option 3: Using CMake
```bash
cd networkstats
mkdir -p build && cd build
cmake ..
make
```

### Option 4: Direct compilation
```bash
g++ -DTEST_MAIN -std=c++11 -Wall -g \
    -I. -I/usr/include -I/usr/local/include \
    ThunderJsonRPCProvider.cpp \
    -o thunder-jsonrpc-provider-test \
    -L/usr/lib -L/usr/local/lib \
    -ljsonrpccpp-client -ljsonrpccpp-common -ljsoncpp
```

## Running the Test

### Prerequisites for Runtime
The NetworkManager Thunder plugin must be running and accessible at:
- **URL**: http://127.0.0.1:9998/jsonrpc
- **Plugin**: org.rdk.NetworkManager.1

### Start the test application
```bash
./thunder-jsonrpc-provider-test
```

### Test Menu
The application provides an interactive menu:

```
========================================
NetworkJsonRPCProvider Test Suite
========================================

Menu:
1. Test getIpv4Address()
2. Test getIpv6Address()
3. Test getConnectionType()
4. Test getDnsEntries()
5. Test populateNetworkData()
6. Test getInterface()
7. Test pingToGatewayCheck()
8. Run all tests
0. Exit
```

### Test Functions

#### 1. getIpv4Address()
- Retrieves IPv4 address for the primary interface
- Also fetches and displays: Gateway, Primary DNS

#### 2. getIpv6Address()
- Retrieves IPv6 address for the primary interface
- Also fetches and displays: Gateway, Primary DNS

#### 3. getConnectionType()
- Determines connection type (Ethernet/WiFi/Unknown)
- Based on interface name pattern

#### 4. getDnsEntries()
- Retrieves DNS server entries (TODO)

#### 5. populateNetworkData()
- Populates network interface data (TODO)

#### 6. getInterface()
- Gets the primary network interface name
- Uses: org.rdk.NetworkManager.1.GetPrimaryInterface

#### 7. pingToGatewayCheck()
- Pings specified endpoint (default: 8.8.8.8)
- Reports: Packet loss, RTT statistics
- Configurable: IP version, count, timeout

#### 8. Run all tests
- Executes all test functions sequentially
- Provides comprehensive output

## NetworkManager JSON-RPC APIs Used

### GetPrimaryInterface
```json
Method: org.rdk.NetworkManager.1.GetPrimaryInterface
Response: {
  "interface": "eth0"
}
```

### GetIPSettings
```json
Method: org.rdk.NetworkManager.1.GetIPSettings
Params: {
  "interface": "eth0",
  "ipversion": "IPv4"
}
Response: {
  "interface": "eth0",
  "ipversion": "IPv4",
  "autoconfig": true,
  "ipaddress": "192.168.1.100",
  "prefix": 24,
  "gateway": "192.168.1.1",
  "primarydns": "8.8.8.8"
}
```

### Ping
```json
Method: org.rdk.NetworkManager.1.Ping
Params: {
  "endpoint": "8.8.8.8",
  "ipversion": "IPv4",
  "count": 5,
  "timeout": 30,
  "guid": "network-stats-ping"
}
Response: {
  "success": true,
  "packetsTransmitted": 5,
  "packetsReceived": 5,
  "packetLoss": "0%",
  "tripMin": "10.5",
  "tripAvg": "12.3",
  "tripMax": "15.2"
}
```

## Troubleshooting

### Build Issues

**Error: `jsonrpccpp/client.h` not found**
- Install libjsonrpccpp development package
- Check include paths in compile command

**Error: `json/json.h` not found**
- Install jsoncpp development package

**Linker errors**
- Verify library paths (-L flags)
- Check that libraries are installed in /usr/lib or /usr/local/lib
- On Ubuntu, libraries are typically in /usr/lib/x86_64-linux-gnu

### Runtime Issues

**Error: Connection refused**
- Ensure NetworkManager Thunder plugin is running
- Check if service is listening on port 9998
- Verify endpoint: http://127.0.0.1:9998/jsonrpc

**Error: Method not found**
- Verify NetworkManager plugin version supports the API
- Check method name: org.rdk.NetworkManager.1.<method>

**Empty responses**
- Check network interface status
- Verify NetworkManager plugin configuration

## Files

- `ThunderJsonRPCProvider.h` - Class header
- `ThunderJsonRPCProvider.cpp` - Implementation with TEST_MAIN
- `CMakeLists.txt` - CMake build configuration
- `Makefile` - Simple makefile
- `build.sh` - Automated build script
- `BUILD.md` - This file

## Clean Up

```bash
# Remove build artifacts
make clean

# Or manually
rm -f thunder-jsonrpc-provider-test
```

## Integration

This test validates the provider class used by NetworkConnectionStats for periodic network diagnostics reporting.
