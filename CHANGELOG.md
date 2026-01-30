# Changelog

All notable changes to this RDK Service will be documented in this file.

* Each RDK Service has a CHANGELOG file that contains all changes done so far. When version is updated, add a entry in the CHANGELOG.md at the top with user friendly information on what was changed with the new version. Please don't mention JIRA tickets in CHANGELOG. 

* Please Add entry in the CHANGELOG for each version change and indicate the type of change with these labels:
    * **Added** for new features.
    * **Changed** for changes in existing functionality.
    * **Deprecated** for soon-to-be removed features.
    * **Removed** for now removed features.
    * **Fixed** for any bug fixes.
    * **Security** in case of vulnerabilities.

* Changes in CHANGELOG should be updated when commits are added to the main or release branches. There should be one CHANGELOG entry per JIRA Ticket. This is not enforced on sprint branches since there could be multiple changes for the same JIRA ticket during development. 

## [1.13.0] - 2026-01-30
### Added
- Implemented IPv4LL support for both Ethernet and WiFi interfaces
- Implemented logic to enable WiFi upon migration
- Fixed crash of Thunder Plugin upon Async WiFiConnect by using GCancellable

## [1.12.0] - 2025-12-23
### Fixed
- Fixed the Ethernet interface enablement upon EntOS migration

## [1.11.0] - 2025-12-19
### Added
- Added support to build for Thunder R5.x

## [1.10.0] - 2025-12-05
### Fixed
- Fixed the GetIPSettings for RDK Backend to return only when all the information like DNS info retrieved
- Fixed the retrieval of psk to pass it to MfrMgr.

## [1.9.0] - 2025-11-21
### Added
- Added shortterm solution to print the AP Info

## [1.8.0] - 2025-11-14
### Fixed
- Removed logging of WiFi PII information

## [1.7.0] - 2025-11-13
### Fixed
- Fixed bug MfrMgr Integration
- Subscribed to WiFi Events even when the device boots with wifi disabled
- Fixed the WiFi signal monitoring as the reporred RSSI was stale data

## [1.6.0] - 2025-10-31
### Added
- Integrated MfrMgr to persist WiFi Credential for DRI & Rollback to RDK-V Backend
- Implemented proper shutdown sequence for NetworkManager Plugin.
- Updated GDbus Support with all the latest fixes that went thro libnm backend

## [1.5.0] - 2025-09-23
### Changed
- Implemented L1/L2 and Part L3 for the plugin
- Implemented IP Address caching for Gnome backend
- Implemented a logic to return appropriate value for GetPrimaryInterface

## [1.4.0] - 2025-09-23
### Changed
- Implemented logic to not to persist SSID when asked
- Implemented IP Address caching to avoid making RPC request to networkmanager when interface status not changed.
- Fixed the Router Discovery app crash when AP does not support UPNP

## [1.3.0] - 2025-09-17
### Added
- Implemented NetworkManagerProxy library which can help Thunder to Create COM-RPC connection to Out-Of-Process plugin
- Implemented Internet Connectivity Monitoring for specific to Primary Interface.
- Updated to not to post onInternetStatusChanged event when secondary interface is disturbed.

## [1.2.0] - 2025-09-15
### Changed
- Reverted NetworkManager to continue to run as out-of-process-plugin

## [1.1.0] - 2025-09-13
### Changed
- Subscribed for Device Addition where driver for wlan0 could be inserted late
- Moved NetworkManager as in-process-plugin
- Update comments across the repo

## [1.0.0] - 2025-08-29
### Added
- Implemented additional L1/L2 for libnm backends
- Addressed Coverity issues in Connectivity Monitoring Thread
- Marking official production release.

## [0.25.0] - 2025-08-27
### Added
- Implemented SetHostname method which can send device's hostname over DHCP explicitly
- Implemented L1/L2 for netsrvmgr (RDK), libnm backends
- Clamped the WIFI Noise value to be within 0 dBm to -96 dBm
- Handled Plugin Termination sequence to stop the monitoring thread before exit
- Handled out-of-process plugin configuration as string
- Fixed Router Discovery to handle dual interface
- Fixed Internet Connectivity Monitoring to not to poll unless requested
- Addressed coverity reported issue

## [0.24.0] - 2025-08-11
### Added
- Use incoming EAP-PrivateKey Password to connect to WiFi when given

## [0.23.0] - 2025-07-30
### Added
- Added L1/L2 workflow for LegacyNetwork, LegacyWiFi and NetworkManager Plugins
- Fixed RemoveKnownSSID method to handle the invalid inputs
- Fixed the logging in GetIPSettings

## [0.22.0] - 2025-07-14
### Fixed
- Fixed trigger point for the connectivity monitoring
- Posting onWiFiStateChange Event when link-up for wifi event is received.

## [0.21.0] - 2025-07-08
### Fixed
- Fixed WiFi On/Off toggling failure
- Fixed the bug in IPAddress availability check upon wake-up
- Removed the SetPrimartInterface method
- Added input param validation for Ping and Stun methods

## [0.20.0] - 2025-06-20
### Fixed
- Fixed the bug in Resetting of Manual IPv4
- Fixed the SSID Name Length Check
- Added Connectivity Improvement
- Fixed the WiFi Signal Monitor thread exception
- Fixed documentation of GetSupportedSecurityModes
- Fixed the WiFi Migration settings

## [0.19.0] - 2025-05-29
### Fixed
- Fixed Persistence related to interface enable/disable status
- Added addtional logs to understand WIFISignal monitoring thread

## [0.18.0] - 2025-05-21
### Fixed
- Fixed onActiveInterfaceChange event data.
- Implemented an improvement to the Ping method to have lesser interval between echo requests

## [0.17.0] - 2025-05-02
### Fixed
- Fixed the memory leak in Gnome-libnm port of the plugin.

## [0.16.0] - 2025-05-01
### Fixed
- Fixed the deregistration of Security Agent upon one successful WPS Connect.
- JSONRPC response for Ping in the legacy plugin is addressed

## [0.15.0] - 2025-04-24
### Fixed
- Implemented a CLI tool to test the plugin independently
- Implemented Retry logic to register with IARM upon failure for RDK Backend
- Fixed the memory leak that was observed in Gnome backend when connecting to SSID
- Fixed the events published upon WPS successful connects and failures.
- Fixed the WiFiConnect call with empty param from the legacy plugin

## [0.14.0] - 2025-04-18
### Added
- Process Monitoring for the out-of-process plugin added
- Added support Enable/Disable Logs to the Gnome NetworkManager daemon
- Fixed publishing of INTERNET Thunder Subsystem.
- Fixed the Memory Leak from the Scanned SSID names
- Fixed Set/Get Primary interface when both Ethernet and WiFi connected.

## [0.13.0] - 2025-04-07
### Changed
- Changed the out-of-process name
- Changed the WIFIConnect API scan & connect if the scan-result is empty
- Changed the GetIPSettings API to check the ipversion passed, and if empty, return IPv4 when available other IPv6.
- Fixed the GetAvailableInterfaces to return only the interfaces that are defined in device.properties
- Enable DEBUG to NetworkManager Gnome daemon when SetLogLevel called with DEBUG.

## [0.12.0] - 2025-03-31
### Added
- The file and folder structure of NetworkManager repo is re-organized
- Implemented a separate library for out-of-process plugin
- Implemented GDbus based APIs for all the NetworkManager APIs
- Implemented a Router Discovery tool
- Changed the approach to identify Open SSIDs
- Added INFO logging for the discovered SSIDs and the security methods discovered.
- Fixed Invalid Timeout in Connectivity Monitoring
- Fixed WiFiConnect Failure for previously saved SSID
- Fixed the missing Events on WPS Connect failure

## [0.11.0] - 2025-03-07
### Added
- Implemented concurrent interface support in public APIs
- Changed the approach to get the SNR value.
- Fixed RemoveKnownSSID() API to clear all the SSIDs when called with empty input
- Fixed minor bug in AddtoKnownSSID() API for Gnome backend
- Fixed Network Reset Flow

## [0.10.0] - 2025-02-24
### Added
- Implemented Security Agent to support WPS
- Implemented WiFi Signal Quality Monitoring using SNR value.
- Implemented L1 unit test code
- Fixed SetPrimaryInterface for Gnome support
- Fixed WiFiConnect failure to connect to unsecured SSIDs
- Fixed GetPrimaryInterface to gracefully handle libnm failure

## [0.9.0] - 2025-01-30
### Added
- Enhanced Internet Connectivity Monitoring
- Redefined the security modes
- Removed explicit API to start/stop connectivity monitoring
- Added retriveSSID method for migration ready
- Implemented initial code for gdbus based methods to communicate with NetworkManager
- Fixed Coverity Issues
- Fixed minor bugs

## [0.8.0] - 2025-01-03
### Changed
- Updated the definition of GetPublicIP method to take interface as additional input
- Updated the Legacy Plugin to use newly defined API

## [0.7.0] - 2024-12-30
### Added
- Gnome WPS custom design implemented
- Handle interface status based on IPAddress change event
- Cleanup STUN client codebase
- Handle JSON Event Subscription
- Handle Platform Init and Configure functions of out-of-process
- Handle the startup order
- Added L1 test cases for the plugins

## [0.6.0] - 2024-12-10
### Added
- Added specific SSID/frequency scanning
- Added retry logic in WPS Connect
- Added Unittest for Connectivity Class
- Fixed Continuous Connectivity Monitoring when no interface connected
- Fixed SetIPSettings on GNome backed

## [0.5.4] - 2024-11-29
### Fixed
- Fixed documentation of NetworkManager
- Handled IPAddressChange event

## [0.5.3] - 2024-11-21
### Fixed
- Fixed GetInterfaceState method
- Fixed bootup events on Gnome backend

## [0.5.2] - 2024-11-19
### Fixed
- Fixed Event data for DefaultInterfacechanged & WiFiThresholdChanged
- Fixed GetAvailableInterfaces on Gnome backend

## [0.5.1] - 2024-11-15
### Added
- Implemented WPS support on Gnome Backend
- Implemented L1/L2 unit test support
### Fixed
- Fixed GetIPSettings error
- Fixed IPAddressChange event

## [0.5.0] - 2024-11-05
### Added
- Enhanced WiFi Connection Structure to support EAP
- Defined JsonEnum to publish the Events
- Refactored Legacy Plugins to use ComRPC to communicate with NetworkManager
- Documentation Update
- Minor bug fixes

## [0.4.0] - 2024-10-04
### Added
- Added RDKLogger Integration
- Fixed Legacy Network API for getIPSetting2
- Includes critical bug fixes 

## [0.3.0] - 2024-09-16
### Added
- Gnome Network Manager support
- Fixed the missing Error events
- Fixed activation & subscription flow
- Includes bug fixes

## [0.2.0] - 2024-05-27
### Added
- Restructured the NetworkManager folder organization
- Refactored Legacy Network & WiFiManager Plugin as JSONRPC
- Built COMRPC Proxy Implementation as part of NetworkManager Plugin itself
- Built out-of-process class as part of main library itself.

## [0.1.0] - 2024-03-28
### Added
- Added NetworkManager plugin. A Unified `NetworkManager` plugin that allows you to manage Ethernet and Wifi interfaces on the device.

