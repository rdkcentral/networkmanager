#############################################################################
# Copyright 2024 RDK Management
# Licensed under the Apache License, Version 2.0
#############################################################################
message ("building libnm test")

set(NM_LIBNM_PROXY_L2_TEST "libnm_proxy_l2_test")

find_package(CURL)
find_package(PkgConfig REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)
pkg_check_modules(GIO REQUIRED gio-2.0)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
FetchContent_MakeAvailable(googletest)

message("networkmanager building with libnm")
add_executable(${NM_LIBNM_PROXY_L2_TEST}
    ${CMAKE_SOURCE_DIR}/tests/l2Test/libnm/l2_test_libnmproxyWifi.cpp
    ${CMAKE_SOURCE_DIR}/tests/l2Test/libnm/l2_test_libnmproxyInit.cpp
    ${CMAKE_SOURCE_DIR}/tests/l2Test/libnm/l2_test_libnmproxy.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/Wraps.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/LibnmWraps.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/GLibWraps.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManager.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerJsonRpc.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerImplementation.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerConnectivity.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerStunClient.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeProxy.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeWIFI.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeEvents.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeUtils.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerSecretAgent.cpp
    ${PROXY_STUB_SOURCES}
)

set_target_properties(${NM_LIBNM_PROXY_L2_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_compile_options(${NM_LIBNM_PROXY_L2_TEST} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)

# Add a definition to handle the NM_DEVICE macro in test
target_compile_definitions(${NM_LIBNM_PROXY_L2_TEST} PRIVATE
    NM_DEVICE_CAST_WORKAROUND=1
)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)
include_directories(${PROJECT_SOURCE_DIR}/legacy)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks/thunder)

target_include_directories(${NM_LIBNM_PROXY_L2_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
)

target_link_options(${NM_LIBNM_PROXY_L2_TEST} PRIVATE
    -g
    -Wall
    -Wl,-wrap,system
    -Wl,-wrap,popen
    -Wl,-wrap,syslog
    -Wl,-wrap,pclose
    -Wl,-wrap,getmntent
    -Wl,-wrap,setmntent
    -Wl,-wrap,v_secure_popen
    -Wl,-wrap,v_secure_pclose
    -Wl,-wrap,v_secure_system
    -Wl,-wrap,nm_device_get_iface
    -Wl,-wrap,nm_client_get_primary_connection
    -Wl,-wrap,nm_active_connection_get_connection
    -Wl,-wrap,nm_connection_get_interface_name
    -Wl,-wrap,nm_client_get_device_by_iface
    -Wl,-wrap,nm_device_get_state
    -Wl,-wrap,nm_client_get_devices
    -Wl,-wrap,nm_client_new
    -Wl,-wrap,nm_device_get_hw_address
    -Wl,-wrap,nm_client_get_active_connections
    -Wl,-wrap,nm_connection_get_setting_connection
    -Wl,-wrap,nm_setting_connection_get_interface_name
    -Wl,-wrap,nm_active_connection_get_ip4_config
    -Wl,-wrap,nm_active_connection_get_ip6_config
    -Wl,-wrap,nm_active_connection_get_dhcp4_config
    -Wl,-wrap,nm_active_connection_get_dhcp6_config
    -Wl,-wrap,nm_connection_get_setting_ip4_config
    -Wl,-wrap,nm_connection_get_setting_ip6_config
    -Wl,-wrap,nm_setting_ip_config_get_method
    -Wl,-wrap,nm_ip_config_get_addresses
    -Wl,-wrap,nm_ip_address_get_address
    -Wl,-wrap,nm_ip_address_get_prefix
    -Wl,-wrap,nm_ip_config_get_gateway
    -Wl,-wrap,nm_ip_config_get_nameservers
    -Wl,-wrap,nm_dhcp_config_get_one_option
    -Wl,-wrap,nm_device_disconnect_async
    -Wl,-wrap,nm_device_disconnect_finish
    -Wl,-wrap,g_main_loop_is_running
    -Wl,-wrap,nm_access_point_get_flags
    -Wl,-wrap,nm_access_point_get_wpa_flags
    -Wl,-wrap,nm_access_point_get_rsn_flags
    -Wl,-wrap,nm_access_point_get_ssid
    -Wl,-wrap,nm_access_point_get_bssid
    -Wl,-wrap,nm_access_point_get_frequency
    -Wl,-wrap,nm_access_point_get_mode
    -Wl,-wrap,nm_access_point_get_max_bitrate
    -Wl,-wrap,nm_access_point_get_strength
    -Wl,-wrap,nm_device_wifi_get_active_access_point
    -Wl,-wrap,nm_device_wifi_request_scan_async
    -Wl,-wrap,nm_device_wifi_request_scan_options_async
    -Wl,-wrap,nm_device_wifi_request_scan_finish
    -Wl,-wrap,nm_client_activate_connection_async
    -Wl,-wrap,nm_connection_get_connection_type
    -Wl,-wrap,nm_connection_get_id
    -Wl,-wrap,nm_client_get_connections
    -Wl,-wrap,nm_device_set_autoconnect
    -Wl,-wrap,nm_device_wifi_get_last_scan
    -Wl,-wrap,nm_client_activate_connection_finish
    -Wl,-wrap,nm_client_add_and_activate_connection_async
    -Wl,-wrap,nm_remote_connection_update2_finish
    -Wl,-wrap,nm_client_add_connection2
    -Wl,-wrap,nm_client_add_connection2_finish
    -Wl,-wrap,nm_client_add_and_activate_connection_finish
    -Wl,-wrap,nm_device_wifi_get_access_points
    -Wl,-wrap,nm_device_get_available_connections
    -Wl,-wrap,nm_object_get_path
    -Wl,-wrap,nm_client_dbus_set_property
    -Wl,-wrap,nm_client_dbus_set_property_finish
    -Wl,-wrap,nm_device_get_active_connection
    -Wl,-wrap,nm_remote_connection_commit_changes
    -Wl,-wrap,nm_setting_ip_config_get_dhcp_hostname
    -Wl,-wrap,nm_setting_ip_config_get_dhcp_send_hostname
    -Wl,-wrap,nm_connection_add_setting
    -Wl,-wrap,nm_setting_wireless_get_ssid
    -Wl,-wrap,nm_connection_get_setting_wireless
    -Wl,-wrap,nm_remote_connection_delete
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_link_libraries(${NM_LIBNM_PROXY_L2_TEST} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${CURL_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBNM_LIBRARIES}
    ${GIO_LIBRARIES}
)

install(TARGETS ${NM_LIBNM_PROXY_L2_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
