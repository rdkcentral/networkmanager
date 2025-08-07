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
. /etc/device.properties

cmd=$1
mode=$2
ifc=$3
flags=$5
file="/tmp/.routerdiscover"
if [[ "$ifc" == "$WIFI_INTERFACE" || "$ifc" == "$ETHERNET_INTERFACE" ]]; then
    if [ "$cmd" == "add" ] && [ "$flags" = "global" ]; then
        if [ ! -f $file ]; then
            touch /tmp/.routerdiscover
            # Find the PID of the routerDiscovery process that is running specifically on this interface.
            pid=$(pgrep -f "routerDiscovery $ifc")
            if [ -n "$pid" ]; then
                echo "$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ") Found existing routerDiscovery ($pid) for $ifc, Killing it." >> /opt/logs/routerInfo.log
                kill -15 "$pid"
            fi
            echo "$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ") Starting routerDiscovery on $ifc" >> /opt/logs/routerInfo.log
            systemctl start routerDiscovery@$ifc
        fi
    fi
fi
