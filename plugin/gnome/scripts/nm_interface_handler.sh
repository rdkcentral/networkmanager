#!/bin/sh
##############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################

# nm_interface_handler.sh
#
# This script manages Ethernet and Wi-Fi interfaces based on marker files.
# If specific marker files exist, it marks eth0 or wlan0 as unmanaged
# in NetworkManager, preventing automatic configuration.
#
# Invocation:
# Called automatically during system startup as part of the thunder-startup-services
# WPEFramework NetworkManager plugin service using ExecStartPre in:
#    wpeframework-networkmanager.service

. /etc/device.properties

# Get BOOT_TYPE
if [ -f /tmp/bootType ]; then
    BOOT_TYPE=$(grep "BOOT_TYPE" /tmp/bootType | cut -d '=' -f 2)
else
    BOOT_TYPE=""
fi

# Ethernet
NSM_ETH_MARKER="/opt/persistent/ethernet_disallowed"
NM_ETH_MARKER="/opt/persistent/ethernet.interface.disable"
ETH_INTERFACE="$ETHERNET_INTERFACE"

# Wi-Fi
NSM_WIFI_MARKER="/opt/persistent/wifi_disallowed"
NM_WIFI_MARKER="/opt/persistent/wifi.interface.disable"
WIFI_INTERFACE="$WIFI_INTERFACE"

if [ "$BOOT_TYPE" = "BOOT_MIGRATION" ]; then
    echo "`/bin/timestamp` :$0: migration boot removing ethernet and wifi disable marker file" >> /opt/logs/NMMonitor.log
    rm -f "$NSM_ETH_MARKER"
    rm -f "$NSM_WIFI_MARKER"
    exit 0
fi

# Rename legacy netsrvmger markers to networkmanager markers if they exist
if [ -f "$NSM_ETH_MARKER" ]; then
    mv "$NSM_ETH_MARKER" "$NM_ETH_MARKER"
fi

if [ -f "$NSM_WIFI_MARKER" ]; then
    mv "$NSM_WIFI_MARKER" "$NM_WIFI_MARKER"
fi

# If networkmanager Ethernet marker exists, make eth0 unmanaged
if [ -f "$NM_ETH_MARKER" ]; then
    echo "`/bin/timestamp` :$0: disabling eth0 interface " >>  /opt/logs/NMMonitor.log
    nmcli device disconnect "$ETH_INTERFACE"
    nmcli dev set "$ETH_INTERFACE" managed no
fi

# If networkmanager Wi-Fi marker exists, make wlan0 unmanaged
if [ -f "$NM_WIFI_MARKER" ]; then
    echo "`/bin/timestamp` :$0: disabling wlan0 interface " >>  /opt/logs/NMMonitor.log
    nmcli device disconnect "$WIFI_INTERFACE"
    nmcli dev set "$WIFI_INTERFACE" managed no
fi
