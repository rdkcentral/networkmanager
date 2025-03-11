#!/bin/sh
##############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

ifc=$1
pid=`pidof upnpdiscover`
if [ -n "$pid" ]; then
    kill -9 $pid
fi 
echo "Starting upnpdiscover on $ifc" >> /opt/logs/routerInfo.log
systemctl start upnpdiscover_interface@$ifc
