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
# If specific marker files exist, it writes a NetworkManager config drop-in
# (/run/NetworkManager/conf.d/20-rdk-iface-disable.conf) that sets matching
# interfaces to managed=0 while their marker files exist.
# This soft setting allows the plugin to re-enable an interface at runtime.
#
# Called by ExecStartPre in NetworkManager.service and
# wpeframework-networkmanager.service.

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

NM_DROPIN_DIR="/run/NetworkManager/conf.d"
NM_DROPIN="$NM_DROPIN_DIR/20-rdk-iface-disable.conf"

mkdir -p "$NM_DROPIN_DIR"
rm -f "$NM_DROPIN"

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

# If networkmanager Ethernet marker exists, set eth0 unmanaged by default
if [ -f "$NM_ETH_MARKER" ]; then
    echo "`/bin/timestamp` :$0: disabling $ETH_INTERFACE interface " >>  /opt/logs/NMMonitor.log
fi

# If networkmanager Wi-Fi marker exists, set wlan0 unmanaged by default
if [ -f "$NM_WIFI_MARKER" ]; then
    echo "`/bin/timestamp` :$0: disabling $WIFI_INTERFACE interface " >>  /opt/logs/NMMonitor.log
fi

if [ -f "$NM_ETH_MARKER" ] || [ -f "$NM_WIFI_MARKER" ]; then
    {
        if [ -f "$NM_ETH_MARKER" ]; then
            echo "[device-rdk-eth]"
            echo "match-device=interface-name:$ETH_INTERFACE"
            echo "managed=0"
            echo ""
        fi
        if [ -f "$NM_WIFI_MARKER" ]; then
            echo "[device-rdk-wifi]"
            echo "match-device=interface-name:$WIFI_INTERFACE"
            echo "managed=0"
            echo ""
        fi
    } >> "$NM_DROPIN"
fi
