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
            pid=`pidof routerdiscover`
            if [ -n "$pid" ]; then
                kill -9 $pid
            fi 
            echo "Starting routerdiscover on $ifc" >> /opt/logs/routerInfo.log
            systemctl start routerdiscover_interface@$ifc
        fi
    fi
fi
