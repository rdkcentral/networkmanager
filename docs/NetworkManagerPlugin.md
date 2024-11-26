<!-- Generated automatically, DO NOT EDIT! -->
<a name="head.NetworkManager_Plugin"></a>
# NetworkManager Plugin

**Version: 0.5.3**

**Status: :white_circle::white_circle::white_circle:**

org.rdk.NetworkManager interface for Thunder framework.

(Defined by [NetworkManager.json](https://github.com/rdkcentral/networkmanager/blob/main/NetworkManager.json))

### Table of Contents

- [Introduction](#head.Introduction)
- [Description](#head.Description)
- [Methods](#head.Methods)
- [Notifications](#head.Notifications)

<a name="head.Introduction"></a>
# Introduction

<a name="head.Scope"></a>
## Scope

This document describes purpose and functionality of the org.rdk.NetworkManager interface (version 0.5.3). It includes detailed specification about its methods provided and notifications sent.

<a name="head.Case_Sensitivity"></a>
## Case Sensitivity

All identifiers of the interfaces described in this document are case-sensitive. Thus, unless stated otherwise, all keywords, entities, properties, relations and actions should be treated as such.

<a name="head.Acronyms,_Abbreviations_and_Terms"></a>
## Acronyms, Abbreviations and Terms

The table below provides and overview of acronyms used in this document and their definitions.

| Acronym | Description |
| :-------- | :-------- |
| <a name="acronym.API">API</a> | Application Programming Interface |
| <a name="acronym.HTTP">HTTP</a> | Hypertext Transfer Protocol |
| <a name="acronym.JSON">JSON</a> | JavaScript Object Notation; a data interchange format |
| <a name="acronym.JSON-RPC">JSON-RPC</a> | A remote procedure call protocol encoded in JSON |

The table below provides and overview of terms and abbreviations used in this document and their definitions.

| Term | Description |
| :-------- | :-------- |
| <a name="term.callsign">callsign</a> | The name given to an instance of a plugin. One plugin can be instantiated multiple times, but each instance the instance name, callsign, must be unique. |

<a name="head.References"></a>
## References

| Ref ID | Description |
| :-------- | :-------- |
| <a name="ref.HTTP">[HTTP](http://www.w3.org/Protocols)</a> | HTTP specification |
| <a name="ref.JSON-RPC">[JSON-RPC](https://www.jsonrpc.org/specification)</a> | JSON-RPC 2.0 specification |
| <a name="ref.JSON">[JSON](http://www.json.org/)</a> | JSON specification |
| <a name="ref.Thunder">[Thunder](https://github.com/WebPlatformForEmbedded/Thunder/blob/master/doc/WPE%20-%20API%20-%20WPEFramework.docx)</a> | Thunder API Reference |

<a name="head.Description"></a>
# Description

A Unified `NetworkManager` plugin that allows you to manage Ethernet and Wifi interfaces on the device.

<a name="head.Methods"></a>
# Methods

The following methods are provided by the org.rdk.NetworkManager interface:

NetworkManager interface methods:

| Method | Description |
| :-------- | :-------- |
| [SetLogLevel](#method.SetLogLevel) | Set Log level for more information |
| [GetLogLevel](#method.GetLogLevel) | Get Log level that is currently used |
| [GetAvailableInterfaces](#method.GetAvailableInterfaces) | Get device supported list of available interface including their state |
| [GetPrimaryInterface](#method.GetPrimaryInterface) | Gets the primary/default network interface for the device |
| [SetPrimaryInterface](#method.SetPrimaryInterface) | Sets the primary/default interface for the device |
| [SetInterfaceState](#method.SetInterfaceState) | Enable or disable the specified interface |
| [GetInterfaceState](#method.GetInterfaceState) | Gets the current Status of the specified interface |
| [GetIPSettings](#method.GetIPSettings) | Gets the IP setting for the given interface |
| [SetIPSettings](#method.SetIPSettings) | Sets the IP settings for the given interface |
| [GetStunEndpoint](#method.GetStunEndpoint) | Get the STUN endpoint that is used to identify public IP of the device |
| [SetStunEndpoint](#method.SetStunEndpoint) | Set the STUN endpoint to be used to identify public IP of the device |
| [GetConnectivityTestEndpoints](#method.GetConnectivityTestEndpoints) | Gets currently used test endpoints |
| [SetConnectivityTestEndpoints](#method.SetConnectivityTestEndpoints) | This method used to set up to 5 endpoints for a connectivity test |
| [IsConnectedToInternet](#method.IsConnectedToInternet) | Seeks whether the device has internet connectivity |
| [GetCaptivePortalURI](#method.GetCaptivePortalURI) | Gets the captive portal URI if connected to any captive portal network |
| [StartConnectivityMonitoring](#method.StartConnectivityMonitoring) | Enable a continuous monitoring of internet connectivity with heart beat interval thats given |
| [StopConnectivityMonitoring](#method.StopConnectivityMonitoring) | Stops the connectivity monitoring |
| [GetPublicIP](#method.GetPublicIP) | Gets the internet/public IP Address of the device |
| [Ping](#method.Ping) | Pings the specified endpoint with the specified number of packets |
| [Trace](#method.Trace) | Traces the specified endpoint with the specified number of packets using `traceroute` |
| [StartWiFiScan](#method.StartWiFiScan) | Initiates WiFi scaning |
| [StopWiFiScan](#method.StopWiFiScan) | Stops WiFi scanning |
| [GetKnownSSIDs](#method.GetKnownSSIDs) | Gets list of saved SSIDs |
| [AddToKnownSSIDs](#method.AddToKnownSSIDs) | Saves the SSID, passphrase, and security mode for upcoming and future sessions |
| [RemoveKnownSSID](#method.RemoveKnownSSID) | Remove given SSID from saved SSIDs |
| [WiFiConnect](#method.WiFiConnect) | Initiates request to connect to the specified SSID with the given passphrase |
| [WiFiDisconnect](#method.WiFiDisconnect) | Disconnects from the currently connected SSID |
| [GetConnectedSSID](#method.GetConnectedSSID) | Returns the connected SSID information |
| [StartWPS](#method.StartWPS) | Initiates a connection using Wifi Protected Setup (WPS) |
| [StopWPS](#method.StopWPS) | Cancels the in-progress WPS pairing operation |
| [GetWiFiSignalStrength](#method.GetWiFiSignalStrength) | Get WiFiSignalStrength of connected SSID |
| [GetSupportedsecurityModes](#method.GetSupportedsecurityModes) | Returns the Wifi security modes that the device supports |
| [GetWifiState](#method.GetWifiState) | Returns the current Wifi State |

<a name="method.SetLogLevel"></a>
## *SetLogLevel [<sup>method</sup>](#head.Methods)*

Set Log level for more information. The possible set log level are as follows. 
* `0`: FATAL  
* `1`: ERROR  
* `2`: WARNING  
* `3`: INFO 
* `4`: DEBUG 
.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.level | integer | Set Log level to get more information |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetLogLevel",
  "params": {
    "level": 1
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetLogLevel"></a>
## *GetLogLevel [<sup>method</sup>](#head.Methods)*

Get Log level that is currently used.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.level | integer | Get Log level to get more information |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetLogLevel"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "level": 1,
    "success": true
  }
}
```

<a name="method.GetAvailableInterfaces"></a>
## *GetAvailableInterfaces [<sup>method</sup>](#head.Methods)*

Get device supported list of available interface including their state.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.interfaces | array | An interface details |
| result.interfaces[#] | object |  |
| result.interfaces[#].type | string | Interface  Type |
| result.interfaces[#].name | string | Interface Name. ex: eth0 or wlan0 |
| result.interfaces[#].mac | string | Interface MAC address |
| result.interfaces[#].enabled | boolean | Whether the interface is currently enabled |
| result.interfaces[#].connected | boolean | Whether the interface is currently connected |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetAvailableInterfaces"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "interfaces": [
      {
        "type": "ETHERNET",
        "name": "eth0",
        "mac": "AA:AA:AA:AA:AA:AA",
        "enabled": true,
        "connected": true
      }
    ],
    "success": true
  }
}
```

<a name="method.GetPrimaryInterface"></a>
## *GetPrimaryInterface [<sup>method</sup>](#head.Methods)*

Gets the primary/default network interface for the device. The active network interface is defined as the one that can make requests to the external network. Returns one of the supported interfaces as per `GetAvailableInterfaces`.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetPrimaryInterface"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "interface": "wlan0"
  }
}
```

<a name="method.SetPrimaryInterface"></a>
## *SetPrimaryInterface [<sup>method</sup>](#head.Methods)*

Sets the primary/default interface for the device. This call fails if the interface is not enabled.

Also see: [onActiveInterfaceChange](#event.onActiveInterfaceChange), [onInterfaceStateChange](#event.onInterfaceStateChange), [onAddressChange](#event.onAddressChange), [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetPrimaryInterface",
  "params": {
    "interface": "wlan0"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.SetInterfaceState"></a>
## *SetInterfaceState [<sup>method</sup>](#head.Methods)*

Enable or disable the specified interface.

Also see: [onInterfaceStateChange](#event.onInterfaceStateChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | Enable the specified interface |
| params.enabled | boolean | Whether the interface must be enabled or disabled |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetInterfaceState",
  "params": {
    "interface": "wlan0",
    "enabled": true
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetInterfaceState"></a>
## *GetInterfaceState [<sup>method</sup>](#head.Methods)*

Gets the current Status of the specified interface.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | Disable the specified interface |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.enabled | boolean | Whether the interface is enabled or disabled |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetInterfaceState",
  "params": {
    "interface": "wlan0"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "enabled": true,
    "success": true
  }
}
```

<a name="method.GetIPSettings"></a>
## *GetIPSettings [<sup>method</sup>](#head.Methods)*

Gets the IP setting for the given interface.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params?.interface | string | <sup>*(optional)*</sup> An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |
| params?.ipversion | string | <sup>*(optional)*</sup> Either IPv4 or IPv6 |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |
| result.ipversion | string | Either IPv4 or IPv6 |
| result.autoconfig | boolean | `true` if DHCP is used, `false` if IP is configured manually |
| result?.dhcpserver | string | <sup>*(optional)*</sup> The DHCP Server address |
| result.ipaddress | string | The IP address |
| result.prefix | integer | The prefix number |
| result.gateway | string | The gateway address |
| result.ula | string | The IPv6 Unified Local Address |
| result.primarydns | string | The primary DNS address |
| result.secondarydns | string | The secondary DNS address |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetIPSettings",
  "params": {
    "interface": "wlan0",
    "ipversion": "IPv4"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "interface": "wlan0",
    "ipversion": "IPv4",
    "autoconfig": true,
    "dhcpserver": "192.168.1.1",
    "ipaddress": "192.168.1.101",
    "prefix": 24,
    "gateway": "192.168.1.1",
    "ula": "d00:410:2016::",
    "primarydns": "192.168.1.1",
    "secondarydns": "192.168.1.2",
    "success": true
  }
}
```

<a name="method.SetIPSettings"></a>
## *SetIPSettings [<sup>method</sup>](#head.Methods)*

Sets the IP settings for the given interface.

Also see: [onAddressChange](#event.onAddressChange), [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |
| params.ipversion | string | Either IPv4 or IPv6 |
| params.autoconfig | boolean | `true` if DHCP is used, `false` if IP is configured manually |
| params.ipaddress | string | The IP address |
| params.prefix | integer | The prefix number |
| params.gateway | string | The gateway address |
| params.primarydns | string | The primary DNS address |
| params.secondarydns | string | The secondary DNS address |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetIPSettings",
  "params": {
    "interface": "wlan0",
    "ipversion": "IPv4",
    "autoconfig": true,
    "ipaddress": "192.168.1.101",
    "prefix": 24,
    "gateway": "192.168.1.1",
    "primarydns": "192.168.1.1",
    "secondarydns": "192.168.1.2"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetStunEndpoint"></a>
## *GetStunEndpoint [<sup>method</sup>](#head.Methods)*

Get the STUN endpoint that is used to identify public IP of the device.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.endpoint | string | The host name or IP address |
| result.port | integer | STUN server port |
| result.timeout | integer | Timeout |
| result.cacheLifetime | integer | STUN server cache timeout |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetStunEndpoint"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "endpoint": "45.57.221.20",
    "port": 3478,
    "timeout": 30,
    "cacheLifetime": 0,
    "success": true
  }
}
```

<a name="method.SetStunEndpoint"></a>
## *SetStunEndpoint [<sup>method</sup>](#head.Methods)*

Set the STUN endpoint to be used to identify public IP of the device.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.endpoint | string | The host name or IP address |
| params.port | integer | STUN server port |
| params?.timeout | integer | <sup>*(optional)*</sup> Timeout |
| params?.cacheLifetime | integer | <sup>*(optional)*</sup> STUN server cache timeout |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetStunEndpoint",
  "params": {
    "endpoint": "45.57.221.20",
    "port": 3478,
    "timeout": 30,
    "cacheLifetime": 0
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetConnectivityTestEndpoints"></a>
## *GetConnectivityTestEndpoints [<sup>method</sup>](#head.Methods)*

Gets currently used test endpoints. on success list out the connectivity test points connections.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.endpoints | array |  |
| result.endpoints[#] | string |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetConnectivityTestEndpoints"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "endpoints": [
      "http://clients3.google.com/generate_204"
    ],
    "success": true
  }
}
```

<a name="method.SetConnectivityTestEndpoints"></a>
## *SetConnectivityTestEndpoints [<sup>method</sup>](#head.Methods)*

This method used to set up to 5 endpoints for a connectivity test. Successful connections are verified with HTTP Status code 204 (No Content).

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.endpoints | array | A list of endpoints to test |
| params.endpoints[#] | string |  |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.SetConnectivityTestEndpoints",
  "params": {
    "endpoints": [
      "http://clients3.google.com/generate_204"
    ]
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.IsConnectedToInternet"></a>
## *IsConnectedToInternet [<sup>method</sup>](#head.Methods)*

Seeks whether the device has internet connectivity. This API might take up to 3s to validate internet connectivity.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params?.ipversion | string | <sup>*(optional)*</sup> Either IPv4 or IPv6 |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.ipversion | string | Either IPv4 or IPv6 |
| result.connected | boolean | `true` if internet connectivity is detected, otherwise `false` |
| result.state | integer | Internet state |
| result.status | string | Internet status |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.IsConnectedToInternet",
  "params": {
    "ipversion": "IPv4"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "ipversion": "IPv4",
    "connected": true,
    "state": 3,
    "status": "FULLY_CONNECTED",
    "success": true
  }
}
```

<a name="method.GetCaptivePortalURI"></a>
## *GetCaptivePortalURI [<sup>method</sup>](#head.Methods)*

Gets the captive portal URI if connected to any captive portal network.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.uri | string | Captive portal URI |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetCaptivePortalURI"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "uri": "http://10.0.0.1/captiveportal.jst",
    "success": true
  }
}
```

<a name="method.StartConnectivityMonitoring"></a>
## *StartConnectivityMonitoring [<sup>method</sup>](#head.Methods)*

Enable a continuous monitoring of internet connectivity with heart beat interval thats given. If the monitoring is already happening, it will be restarted with new given interval. When the interval is not passed, it will be 60s by default.

Also see: [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params?.interval | integer | <sup>*(optional)*</sup> Interval in sec |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StartConnectivityMonitoring",
  "params": {
    "interval": 30
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.StopConnectivityMonitoring"></a>
## *StopConnectivityMonitoring [<sup>method</sup>](#head.Methods)*

Stops the connectivity monitoring.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StopConnectivityMonitoring"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetPublicIP"></a>
## *GetPublicIP [<sup>method</sup>](#head.Methods)*

Gets the internet/public IP Address of the device.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object | It allows empty parameter too |
| params?.ipversion | string | <sup>*(optional)*</sup> Either IPv4 or IPv6 |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.ipaddress | string | The IP address |
| result.ipversion | string | Either IPv4 or IPv6 |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetPublicIP",
  "params": {
    "ipversion": "IPv4"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "ipaddress": "192.168.1.101",
    "ipversion": "IPv4",
    "success": true
  }
}
```

<a name="method.Ping"></a>
## *Ping [<sup>method</sup>](#head.Methods)*

Pings the specified endpoint with the specified number of packets.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.endpoint | string | The host name or IP address |
| params.ipversion | string | Either IPv4 or IPv6 |
| params?.count | integer | <sup>*(optional)*</sup> The number of requests to send. Default is 3 |
| params?.timeout | integer | <sup>*(optional)*</sup> Timeout |
| params?.guid | string | <sup>*(optional)*</sup> The globally unique identifier |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.endpoint | string | The host name or IP address |
| result.packetsTransmitted | integer | The number of packets sent |
| result.packetsReceived | integer | The number of packets received |
| result.packetLoss | string | The number of packets lost |
| result.tripMin | string | The minimum amount of time to receive the packets |
| result.tripAvg | string | The average time to receive the packets |
| result.tripMax | string | The maximum amount of time to receive the packets |
| result.tripStdDev | string | The standard deviation for the trip |
| result.error | string | An error message |
| result.guid | string | The globally unique identifier |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.Ping",
  "params": {
    "endpoint": "45.57.221.20",
    "ipversion": "IPv4",
    "count": 10,
    "timeout": 30,
    "guid": "..."
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "endpoint": "45.57.221.20",
    "packetsTransmitted": 10,
    "packetsReceived": 10,
    "packetLoss": "0.0",
    "tripMin": "61.264",
    "tripAvg": "130.397",
    "tripMax": "230.832",
    "tripStdDev": "80.919",
    "error": "...",
    "guid": "...",
    "success": true
  }
}
```

<a name="method.Trace"></a>
## *Trace [<sup>method</sup>](#head.Methods)*

Traces the specified endpoint with the specified number of packets using `traceroute`.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.endpoint | string | The host name or IP address |
| params.ipversion | string | Either IPv4 or IPv6 |
| params?.packets | integer | <sup>*(optional)*</sup> The number of packets to send. Default is 5 |
| params?.guid | string | <sup>*(optional)*</sup> The globally unique identifier |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.endpoint | string | The host name or IP address |
| result.results | string | The response of traceroute |
| result.guid | string | The globally unique identifier |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.Trace",
  "params": {
    "endpoint": "45.57.221.20",
    "ipversion": "IPv4",
    "packets": 10,
    "guid": "..."
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "endpoint": "45.57.221.20",
    "results": "...",
    "guid": "...",
    "success": true
  }
}
```

<a name="method.StartWiFiScan"></a>
## *StartWiFiScan [<sup>method</sup>](#head.Methods)*

Initiates WiFi scaning. This method supports scanning for specific range of frequency like 2.4GHz only or 5GHz only or 6GHz only or ALL. When no input passed about the frequency to be scanned, it scans for all. When list of SSIDs to be scanned specifically, it can be passed as input. It publishes 'onAvailableSSIDs' event upon completion.

Also see: [onAvailableSSIDs](#event.onAvailableSSIDs)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params?.frequency | string | <sup>*(optional)*</sup> The frequency to scan. An empty or `null` value scans all frequencies |
| params?.ssids | array | <sup>*(optional)*</sup> The list of SSIDs to be scanned |
| params?.ssids[#] | string | <sup>*(optional)*</sup>  |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StartWiFiScan",
  "params": {
    "frequency": "5",
    "ssids": [
      "Xfinity Mobile"
    ]
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.StopWiFiScan"></a>
## *StopWiFiScan [<sup>method</sup>](#head.Methods)*

Stops WiFi scanning. Any discovered SSIDs from the call to the `StartWiFiScan` method up to the point where this method is called are still returned as event.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StopWiFiScan"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetKnownSSIDs"></a>
## *GetKnownSSIDs [<sup>method</sup>](#head.Methods)*

Gets list of saved SSIDs. This method returns all the SSIDs that are saved as array.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.ssids | array | Known SSIDS |
| result.ssids[#] | string |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetKnownSSIDs"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "ssids": [
      "Xfinity_Guest"
    ],
    "success": true
  }
}
```

<a name="method.AddToKnownSSIDs"></a>
## *AddToKnownSSIDs [<sup>method</sup>](#head.Methods)*

Saves the SSID, passphrase, and security mode for upcoming and future sessions. This method only adds to the persistent memory; does not disconnect from currently connected SSID.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.ssid | string | The paired SSID |
| params.passphrase | string | The access point password |
| params.security | integer | The security mode. See `getSupportedsecurityModes` |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.AddToKnownSSIDs",
  "params": {
    "ssid": "123412341234",
    "passphrase": "password",
    "security": 6
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.RemoveKnownSSID"></a>
## *RemoveKnownSSID [<sup>method</sup>](#head.Methods)*

Remove given SSID from saved SSIDs. This method just removes an entry from the list and of the list is having only one entry thats being removed, it will initiate a disconnect.

Also see: [onWiFiStateChange](#event.onWiFiStateChange), [onAddressChange](#event.onAddressChange), [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.ssid | string | The paired SSID |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.RemoveKnownSSID",
  "params": {
    "ssid": "123412341234"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.WiFiConnect"></a>
## *WiFiConnect [<sup>method</sup>](#head.Methods)*

Initiates request to connect to the specified SSID with the given passphrase. Passphrase can be `null` when the network security is `NONE`. When called with no arguments, this method attempts to connect to the saved SSID and password. See `AddToKnownSSIDs`.

Also see: [onWiFiStateChange](#event.onWiFiStateChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.ssid | string | The paired SSID |
| params.passphrase | string | The access point password |
| params.security | integer | The security mode. See `getSupportedsecurityModes` |
| params?.ca_cert | string | <sup>*(optional)*</sup> The ca_cert to be used for EAP |
| params?.client_cert | string | <sup>*(optional)*</sup> The client_cert to be used for EAP |
| params?.private_key | string | <sup>*(optional)*</sup> The private_key to be used for EAP |
| params?.private_key_passwd | string | <sup>*(optional)*</sup> The private_key_passwd to be used for EAP |
| params?.eap | string | <sup>*(optional)*</sup> The EAP type to be used |
| params?.eap_identity | string | <sup>*(optional)*</sup> The identity to be used for EAP |
| params?.eap_password | string | <sup>*(optional)*</sup> The eap_password to be used for EAP |
| params?.eap_phase1 | string | <sup>*(optional)*</sup> The eap_phase1 to be used for EAP |
| params?.eap_phase2 | string | <sup>*(optional)*</sup> The eap_phase2 to be used for EAP |
| params?.persist | boolean | <sup>*(optional)*</sup>  To persist the SSID across reboots; similar to auto connect |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.WiFiConnect",
  "params": {
    "ssid": "123412341234",
    "passphrase": "password",
    "security": 6,
    "ca_cert": "...",
    "client_cert": "...",
    "private_key": "...",
    "private_key_passwd": "...",
    "eap": "TLS",
    "eap_identity": "...",
    "eap_password": "...",
    "eap_phase1": "...",
    "eap_phase2": "...",
    "persist": true
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.WiFiDisconnect"></a>
## *WiFiDisconnect [<sup>method</sup>](#head.Methods)*

Disconnects from the currently connected SSID. A event will be posted upon completion.

Also see: [onWIFIStateChange](#event.onWIFIStateChange), [onAddressChange](#event.onAddressChange), [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.WiFiDisconnect"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetConnectedSSID"></a>
## *GetConnectedSSID [<sup>method</sup>](#head.Methods)*

Returns the connected SSID information.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.ssid | string | The paired SSID |
| result.bssid | string | The paired BSSID |
| result.security | string | The security mode. See the `connect` method |
| result.strength | string | The RSSI value in dBm |
| result.frequency | string | The supported frequency for this SSID in GHz |
| result.rate | string | The physical data rate in Mbps |
| result.noise | string | The average noise strength in dBm |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetConnectedSSID"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "ssid": "123412341234",
    "bssid": "ff:ff:ff:ff:ff:ff",
    "security": "5",
    "strength": "-27.000000",
    "frequency": "2.442000",
    "rate": "144.000000",
    "noise": "-121.000000",
    "success": true
  }
}
```

<a name="method.StartWPS"></a>
## *StartWPS [<sup>method</sup>](#head.Methods)*

Initiates a connection using Wifi Protected Setup (WPS). An existing connection will be disconnected before attempting to initiate a new connection. Failure in WPS pairing will trigger an error event.

If the `method` parameter is set to `SERIALIZED_PIN`, then RDK retrieves the serialized pin using the Manufacturer (MFR) API. If the `method` parameter is set to `PIN`, then RDK use the pin supplied as part of the request. If the `method` parameter is set to `PBC`, then RDK uses Push Button Configuration (PBC) to obtain the pin.

Also see: [onWIFIStateChange](#event.onWIFIStateChange), [onAddressChange](#event.onAddressChange), [onInternetStatusChange](#event.onInternetStatusChange)

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.method | string | The method used to obtain the pin (must be one of the following: PBC=0, PIN=1, SERIALIZED_PIN=2) |
| params?.pin | string | <sup>*(optional)*</sup> A valid 8 digit WPS pin number. Use this parameter when the `method` parameter is set to `PIN` |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result?.pin | string | <sup>*(optional)*</sup> The WPS pin value. Valid only when `method` is set to `PIN` or `SERIALIZED_PIN` |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StartWPS",
  "params": {
    "method": "PIN",
    "pin": "88888888"
  }
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "pin": "88888888",
    "success": true
  }
}
```

<a name="method.StopWPS"></a>
## *StopWPS [<sup>method</sup>](#head.Methods)*

Cancels the in-progress WPS pairing operation. The operation forcefully stops the in-progress pairing attempt and aborts the current scan. WPS pairing must be in-progress for the operation to succeed.

Also see: [onWIFIStateChange](#event.onWIFIStateChange)

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.StopWPS"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "success": true
  }
}
```

<a name="method.GetWiFiSignalStrength"></a>
## *GetWiFiSignalStrength [<sup>method</sup>](#head.Methods)*

Get WiFiSignalStrength of connected SSID.

Also see: [onWiFiSignalStrengthChange](#event.onWiFiSignalStrengthChange)

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.ssid | string | The paired SSID |
| result.strength | string | The RSSI value in dBm |
| result.quality | integer | Signal strength Quality |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetWiFiSignalStrength"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "ssid": "123412341234",
    "strength": "-27.000000",
    "quality": 123,
    "success": true
  }
}
```

<a name="method.GetSupportedsecurityModes"></a>
## *GetSupportedsecurityModes [<sup>method</sup>](#head.Methods)*

Returns the Wifi security modes that the device supports.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.security_modes | object | The supported security modes and its associated integer value |
| result.security_modes?.NET_WIFI_SECURITY_NONE | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WEP_64 | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WEP_128 | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_PSK_TKIP | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_PSK_AES | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA2_PSK_TKIP | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA2_PSK_AES | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_ENTERPRISE_TKIP | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_ENTERPRISE_AES | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA2_ENTERPRISE_TKIP | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_WPA2_PSK | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA3_PSK_AES | integer | <sup>*(optional)*</sup>  |
| result.security_modes?.NET_WIFI_SECURITY_WPA3_SAE | integer | <sup>*(optional)*</sup>  |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetSupportedsecurityModes"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "security_modes": {
      "NET_WIFI_SECURITY_NONE": 0,
      "NET_WIFI_SECURITY_WEP_64": 1,
      "NET_WIFI_SECURITY_WEP_128": 2,
      "NET_WIFI_SECURITY_WPA_PSK_TKIP": 3,
      "NET_WIFI_SECURITY_WPA_PSK_AES": 4,
      "NET_WIFI_SECURITY_WPA2_PSK_TKIP": 5,
      "NET_WIFI_SECURITY_WPA2_PSK_AES": 6,
      "NET_WIFI_SECURITY_WPA_ENTERPRISE_TKIP": 7,
      "NET_WIFI_SECURITY_WPA_ENTERPRISE_AES": 8,
      "NET_WIFI_SECURITY_WPA2_ENTERPRISE_TKIP": 9,
      "NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES": 10,
      "NET_WIFI_SECURITY_WPA_WPA2_PSK": 11,
      "NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE": 12,
      "NET_WIFI_SECURITY_WPA3_PSK_AES": 13,
      "NET_WIFI_SECURITY_WPA3_SAE": 14
    },
    "success": true
  }
}
```

<a name="method.GetWifiState"></a>
## *GetWifiState [<sup>method</sup>](#head.Methods)*

Returns the current Wifi State. The possible Wifi states are as follows.  
**Wifi States**  
* `0`: WIFI_STATE_UNINSTALLED - The device was in an installed state and was uninstalled; or, the device does not have a Wifi radio installed   
* `1`: WIFI_STATE_DISABLED - The device is installed but not yet enabled  
* `2`: WIFI_STATE_DISCONNECTED - The device is installed and enabled, but not yet connected to a network  
* `3`: WIFI_STATE_PAIRING - The device is in the process of pairing, but not yet connected to a network  
* `4`: WIFI_STATE_CONNECTING - The device is attempting to connect to a network  
* `5`: WIFI_STATE_CONNECTED - The device is successfully connected to a network  
* `6`: WIFI_STATE_SSID_NOT_FOUND - The requested SSID to connect is not found 
* `7`: WIFI_STATE_SSID_CHANGED - The device connected SSID is changed 
* `8`: WIFI_STATE_CONNECTION_LOST - The device network connection is lost 
* `9`: WIFI_STATE_CONNECTION_FAILED - The device connection got failed 
* `10`: WIFI_STATE_CONNECTION_INTERRUPTED - The device connection is interrupted 
* `11`: WIFI_STATE_INVALID_CREDENTIALS - The credentials provided to connect is not valid 
* `12`: WIFI_STATE_AUTHENTICATION_FAILED - Authentication process as a whole could not be successfully completed 
* `13`: WIFI_STATE_ERROR - The device has encountered an unrecoverable error with the Wifi adapter.

### Parameters

This method takes no parameters.

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | object |  |
| result.state | integer | The given State |
| result.status | string | WiFi status |
| result.success | boolean | Whether the request succeeded |

### Example

#### Request

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "org.rdk.NetworkManager.1.GetWifiState"
}
```

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "state": 4,
    "status": "WIFI_STATE_CONNECTED",
    "success": true
  }
}
```

<a name="head.Notifications"></a>
# Notifications

Notifications are autonomous events triggered by the internals of the implementation and broadcasted via JSON-RPC to all registered observers. Refer to [[Thunder](#ref.Thunder)] for information on how to register for a notification.

The following events are provided by the org.rdk.NetworkManager interface:

NetworkManager interface events:

| Event | Description |
| :-------- | :-------- |
| [onInterfaceStateChange](#event.onInterfaceStateChange) | Triggered when an interface state is changed |
| [onAddressChange](#event.onAddressChange) | Triggered when an IP Address is assigned or lost |
| [onActiveInterfaceChange](#event.onActiveInterfaceChange) | Triggered when the primary/active interface changes, regardless if it's from a system operation or through the `SetPrimaryInterface` method |
| [onInternetStatusChange](#event.onInternetStatusChange) | Triggered when internet connection state changed |
| [onAvailableSSIDs](#event.onAvailableSSIDs) | Triggered when scan completes or when scan cancelled |
| [onWiFiStateChange](#event.onWiFiStateChange) | Triggered when WIFI connection state get changed |
| [onWiFiSignalStrengthChange](#event.onWiFiSignalStrengthChange) | Triggered when WIFI connection Signal Strength get changed |

<a name="event.onInterfaceStateChange"></a>
## *onInterfaceStateChange [<sup>event</sup>](#head.Notifications)*

Triggered when an interface state is changed. The possible states are 
* 'INTERFACE_ADDED' 
* 'INTERFACE_LINK_UP' 
* 'INTERFACE_LINK_DOWN' 
* 'INTERFACE_ACQUIRING_IP' 
* 'INTERFACE_REMOVED' 
* 'INTERFACE_DISABLED' 
.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |
| params.state | integer | Current state of the interface |
| params.status | string | Current status of the interface |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onInterfaceStateChange",
  "params": {
    "interface": "wlan0",
    "state": 1,
    "status": "INTERFACE_LINK_UP"
  }
}
```

<a name="event.onAddressChange"></a>
## *onAddressChange [<sup>event</sup>](#head.Notifications)*

Triggered when an IP Address is assigned or lost.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.interface | string | An interface, such as `eth0` or `wlan0`, depending upon availability of the given interface |
| params.ipaddress | string | The IP address |
| params.ipversion | string | Either IPv4 or IPv6 |
| params.status | string | Whether IP address was acquired or lost (must be one of the following: 'ACQUIRED', 'LOST') |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onAddressChange",
  "params": {
    "interface": "wlan0",
    "ipaddress": "192.168.1.101",
    "ipversion": "IPv4",
    "status": "ACQUIRED"
  }
}
```

<a name="event.onActiveInterfaceChange"></a>
## *onActiveInterfaceChange [<sup>event</sup>](#head.Notifications)*

Triggered when the primary/active interface changes, regardless if it's from a system operation or through the `SetPrimaryInterface` method.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.prevActiveInterface | string | The previous interface that was changed |
| params.activeInterface | string | The current interface |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onActiveInterfaceChange",
  "params": {
    "prevActiveInterface": "wlan0",
    "activeInterface": "eth0"
  }
}
```

<a name="event.onInternetStatusChange"></a>
## *onInternetStatusChange [<sup>event</sup>](#head.Notifications)*

Triggered when internet connection state changed.The possible internet connection status are `NO_INTERNET`, `LIMITED_INTERNET`, `CAPTIVE_PORTAL`, `FULLY_CONNECTED`.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.prevState | integer | The privious internet connection state |
| params.prevStatus | string | The previous internet connection status |
| params.state | integer | The internet connection state |
| params.status | string | The internet connection status |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onInternetStatusChange",
  "params": {
    "prevState": 1,
    "prevStatus": "NO_INTERNET",
    "state": 4,
    "status": "FULLY_CONNECTED"
  }
}
```

<a name="event.onAvailableSSIDs"></a>
## *onAvailableSSIDs [<sup>event</sup>](#head.Notifications)*

Triggered when scan completes or when scan cancelled.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.ssids | array | On Available SSID's |
| params.ssids[#] | object |  |
| params.ssids[#].ssid | string | Ssid |
| params.ssids[#].security | integer | Security |
| params.ssids[#].strength | string | Strength |
| params.ssids[#].frequency | string | Frequency |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onAvailableSSIDs",
  "params": {
    "ssids": [
      {
        "ssid": "myAP-2.4",
        "security": 6,
        "strength": "-27.000000",
        "frequency": "2.442000"
      }
    ]
  }
}
```

<a name="event.onWiFiStateChange"></a>
## *onWiFiStateChange [<sup>event</sup>](#head.Notifications)*

Triggered when WIFI connection state get changed. The possible states are defined in `GetWifiState()`.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.state | integer | WiFi State |
| params.status | string | WiFi status |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onWiFiStateChange",
  "params": {
    "state": 5,
    "status": "WIFI_STATE_CONNECTED"
  }
}
```

<a name="event.onWiFiSignalStrengthChange"></a>
## *onWiFiSignalStrengthChange [<sup>event</sup>](#head.Notifications)*

Triggered when WIFI connection Signal Strength get changed.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.ssid | string | Signal Strength changed SSID |
| params.strength | string | Signal Strength |
| params.quality | string | Signal quality |

### Example

```json
{
  "jsonrpc": "2.0",
  "method": "client.events.1.onWiFiSignalStrengthChange",
  "params": {
    "ssid": "home-new_123",
    "strength": "-27.000000",
    "quality": "Excellent"
  }
}
```

