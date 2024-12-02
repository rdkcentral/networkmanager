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
