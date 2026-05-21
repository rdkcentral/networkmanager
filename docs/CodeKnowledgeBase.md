# NetworkManager Code Knowledge Base

## Purpose
This repository implements the Thunder plugin `org.rdk.NetworkManager` as a unified network + Wi-Fi control surface, with compile-time backend selection:
- GNOME NetworkManager backend (`libnm` or `gdbus` path)
- RDK NetSrvMgr backend (`IARM` path)

The plugin exposes JSON-RPC and COM-RPC while keeping platform logic in an out-of-process implementation.

## High-Level Architecture
The runtime has two major parts:
1. In-process Thunder-facing plugin wrapper (`plugin/NetworkManager.cpp` + `plugin/NetworkManagerJsonRpc.cpp`)
2. Out-of-process implementation (`plugin/NetworkManagerImplementation.cpp`) that performs all network operations

Shared contract lives in `interface/INetworkManager.h`.

## Key Module Map

### Contract + API Definition
- `interface/INetworkManager.h`: RPC interface, enums, data structs, events, and iterator types
- `definition/NetworkManager.json`: JSON-RPC schema used for API generation/docs
- `docs/NetworkManagerPlugin.md`: generated API documentation (v2.2.0)

### Core Plugin Wrapper
- `plugin/NetworkManager.h`: plugin class, notification bridge, JSON-RPC method declarations
- `plugin/NetworkManager.cpp`: lifecycle (`Initialize`, `Deinitialize`), COM-RPC root setup, method registration flow
- `plugin/NetworkManagerJsonRpc.cpp`: JSON-RPC method registration/unregistration and parameter translation
- `plugin/Module.cpp`: module name declaration

### Out-of-Process Implementation
- `plugin/NetworkManagerImplementation.h`: full INetworkManager implementation, config classes, state/cache/thread members
- `plugin/NetworkManagerImplementation.cpp`: config parsing, connectivity/STUN logic, ping/trace execution, monitor thread orchestration
- `plugin/NetworkManagerConnectivity.cpp`: connectivity probing and internet state tracking
- `plugin/NetworkManagerStunClient.cpp`: public IP discovery via STUN
- `plugin/NetworkManagerLogger.cpp`, `plugin/NetworkManagerLogger.h`: plugin logging abstraction

### Backend-Specific Implementations
- `plugin/gnome/NetworkManagerGnomeProxy.cpp`: GNOME backend `platform_init/platform_deinit`, interface operations through `libnm`
- `plugin/gnome/gdbus/NetworkManagerGdbusProxy.cpp`: GNOME gdbus variant
- `plugin/rdk/NetworkManagerRDKProxy.cpp`: RDK NetSrvMgr + IARM event registration and API calls

### Legacy Compatibility Plugins
- `legacy/LegacyNetworkAPIs.cpp`
- `legacy/LegacyWiFiManagerAPIs.cpp`

### Tooling
- `tools/plugincli/*`: standalone CLI test helpers for GNOME backend code paths
- `tools/upnp/*`: router discovery utilities and scripts
- `tools/nflog/src/nflog.c`: NFLOG listener tool
- `tools/nflog/src/check_neighbor.c`: long-running IPv4/IPv6 neighbor table monitor for `eth0`/`wlan0`

### Tests
- `tests/l1Test/*`: class-level tests (connectivity, STUN, optional router discovery)
- `tests/l2Test/*`: integration-like tests split by backend and legacy coverage
- `tests/mocks/*`: wrappers and mocks for GLib/libnm/IARM/other dependencies

## Build and Feature Flags
Top-level options in `CMakeLists.txt`:
- `ENABLE_LEGACY_PLUGINS` (default ON)
- `USE_RDK_LOGGER`
- `ENABLE_UNIT_TESTING`
- `USE_TELEMETRY`

Plugin/backend options in `plugin/CMakeLists.txt` and `tools/CMakeLists.txt`:
- `ENABLE_GNOME_NETWORKMANAGER`
- `ENABLE_GNOME_GDBUS`
- `ENABLE_MIGRATION_MFRMGR_SUPPORT`
- `ENABLE_PLUGIN_CLI`
- `ENABLE_ROUTER_DISCOVERY_TOOL`
- `ENABLE_NFLOG_LISTENER`

Key build products:
- Main plugin shared lib: `${NAMESPACE}NetworkManager`
- Out-of-process impl lib: `${NAMESPACE}NetworkManagerImpl`

## Runtime Flow (Mental Model)
1. Thunder activates `NetworkManager` plugin wrapper.
2. Wrapper calls `service->Root<Exchange::INetworkManager>(..., "NetworkManagerImplementation")`.
3. Out-of-process implementation starts and parses config (`Configure`).
4. `platform_init()` selects backend code (GNOME or RDK) and subscribes to network events.
5. JSON-RPC methods in wrapper validate/translate request fields and delegate to COM-RPC `INetworkManager` calls.
6. Backend events are re-published to Thunder clients through `INetworkManager::INotification` and JSON-RPC event notifications.

## API Layer Responsibilities
- `plugin/NetworkManagerJsonRpc.cpp` is the primary JSON contract enforcement layer.
- It handles:
  - Method registration names
  - Argument presence and basic validation
  - Enum/string conversion
  - Response shaping (`success` + payload fields)
- Business logic remains in `NetworkManagerImplementation*` and backend files.

## Concurrency and Lifecycle Notes
- Multiple threads are used for registration retries, process monitoring, and Wi-Fi signal monitoring.
- GNOME backend creates an isolated `GMainContext` for `NMClient` to avoid unsafe cross-thread context interaction.
- Deinitialization path should always:
  - unregister notifications,
  - stop monitor threads,
  - release COM-RPC interfaces,
  - terminate remote connection if active.

## Where to Add New Features
When adding a new API method/event:
1. Add types/signatures in `interface/INetworkManager.h`.
2. Update `definition/NetworkManager.json`.
3. Add JSON-RPC binding in `plugin/NetworkManager.h` and `plugin/NetworkManagerJsonRpc.cpp`.
4. Implement core logic in `plugin/NetworkManagerImplementation.h/.cpp`.
5. Add backend-specific behavior in:
   - `plugin/gnome/*` and/or
   - `plugin/rdk/NetworkManagerRDKProxy.cpp`.
6. Add/extend tests in `tests/l1Test` and relevant `tests/l2Test` backend folder.

## Practical Gotchas
- Interface assumptions are often hardcoded to `eth0` and `wlan0` in multiple paths.
- Validation behavior differs between wrapper layer and backend layer; keep checks aligned.
- Build flag combinations significantly change compiled sources (especially GNOME vs RDK and gdbus vs libnm).
- Persistent/network migration scripts and service-dropins are installed conditionally for GNOME backend.

## Quick Onboarding Checklist
1. Read `README.md` and `docs/NetworkManagerPlugin.md` for external behavior.
2. Read `interface/INetworkManager.h` to understand the stable RPC contract.
3. Follow `plugin/NetworkManager.cpp` init/deinit flow.
4. Inspect `plugin/NetworkManagerJsonRpc.cpp` to map JSON methods to RPC calls.
5. Trace one method end-to-end into `NetworkManagerImplementation.cpp`.
6. Check selected backend implementation file (`gnome` or `rdk`) based on build flags.
7. Run/extend `tests/l1Test` first, then backend-specific `tests/l2Test`.