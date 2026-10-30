#!/bin/sh

# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

#!/bin/sh

INTERFACE="${1:-wlan0}"
CONFIG_FILE="/opt/secure/wifi/wpa_supplicant.conf"
DISABLE_FILE="/opt/persistent/wifi.interface.disable"

# Do not apply parameters if the Wi-Fi disable flag file is present
[ -f "$DISABLE_FILE" ] && exit
# Ensure the config file exists
[ -f "$CONFIG_FILE" ] || exit 

sed '/^network=/,$d' "$CONFIG_FILE" | while IFS='=' read -r key val; do
    case "$key" in
        '#'*|'') continue ;;
    esac

    if [ -n "$key" ] && [ -n "$val" ]; then
        wpa_cli -i "$INTERFACE" set "$key" "$val"
    fi
done
