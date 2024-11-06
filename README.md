# NetworkManager Plugin
This new Unified NetworkManager Thunder plugin is responsible for configuring and managing all Networking and WiFi interfaces on any given device. This new plugin will be replacing the individual legacy Network and WiFiManager Thunder Plugins that works over IARM.

# Design
The new Unified NetworkManager Thunder plugin is designed provides both COMRPC and JSONRPC support over Thunder and it supports multiple backends like RDK Network Service Manager and Gnome NetworkManager which can be defined at compile time. The defined APIs/Methods works consistently across Gnome NetworkManager and RDK's Network Service Manager.

The plugin implements all the core logic in a out-of-process which communicates over dbus to either Gnome NetworkManager or RDK Network Service Manager.

# API Documentation
Please refer [NetworkManager](docs/NetworkManagerPlugin.md) documentation for API specification.

The documentation for legacy [Network](https://github.com/rdkcentral/rdkservices/blob/main/docs/api/NetworkPlugin.md) and [WiFiManager](https://github.com/rdkcentral/rdkservices/blob/main/docs/api/WifiPlugin.md) plugins are here.

## Release ##
There are 2 active branches namely `develop` and `main`. As the name conveys, `develop` branch is for active development and for contributions.
The Plugin workflow verifies every PR and also the team verifies the plugin on the RPi and RDK Device. The verified changes are taken to `main` branch and releases will be made from there

## Questions? ##
If you have any questions or concerns reach out to [Jacob Gladish](mailto:Jacob_Gladish@cable.comcast.com) and [Karunakaran Amirthalingam](mailto:karunakaran_amirthalingam@comcast.com). 

