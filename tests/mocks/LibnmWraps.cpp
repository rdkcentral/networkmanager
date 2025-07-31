#include "LibnmWraps.h"
#include <gmock/gmock.h>

extern "C" const char* __wrap_nm_device_get_hw_address(NMDevice* device) {
    return LibnmWraps::getInstance().nm_device_get_hw_address(device);
}

extern "C" NMClient* __wrap_nm_client_new(GCancellable* cancellable, GError** error) {
    return LibnmWraps::getInstance().nm_client_new(cancellable, error);
}

extern "C" const char* __wrap_nm_device_get_iface(NMDevice* device) {
    return LibnmWraps::getInstance().nm_device_get_iface(device);
}

extern "C" NMActiveConnection* __wrap_nm_client_get_primary_connection(NMClient *client) {
    return LibnmWraps::getInstance().nm_client_get_primary_connection(client);
}

extern "C" NMRemoteConnection* __wrap_nm_active_connection_get_connection(NMActiveConnection *connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_connection(connection);
}

extern "C" const char* __wrap_nm_connection_get_interface_name(NMRemoteConnection *connection) {
    return LibnmWraps::getInstance().nm_connection_get_interface_name(connection);
}

extern "C" NMDevice* __wrap_nm_client_get_device_by_iface(NMClient *client, const char *iface) {
    return LibnmWraps::getInstance().nm_client_get_device_by_iface(client, iface);
}

extern "C" NMDeviceState __wrap_nm_device_get_state(NMDevice *device) {
    return LibnmWraps::getInstance().nm_device_get_state(device);
}

extern "C" void __wrap_nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    LibnmWraps::getInstance().nm_device_disconnect_async(device, cancellable, callback, user_data);
}

extern "C" gboolean __wrap_nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error) {
    return LibnmWraps::getInstance().nm_device_disconnect_finish(device, result, error);
}

extern "C" const GPtrArray* __wrap_nm_client_get_devices(NMClient* client) {
    return LibnmWraps::getInstance().nm_client_get_devices(client);
}

extern "C" const GPtrArray* __wrap_nm_client_get_active_connections(NMClient* client) {
    return LibnmWraps::getInstance().nm_client_get_active_connections(client);
}

extern "C" NMSettingIPConfig* __wrap_nm_connection_get_setting_ip4_config(NMConnection* connection) {
    return LibnmWraps::getInstance().nm_connection_get_setting_ip4_config(connection);
}

extern "C" NMSettingIPConfig* __wrap_nm_connection_get_setting_ip6_config(NMConnection* connection) {
    return LibnmWraps::getInstance().nm_connection_get_setting_ip6_config(connection);
}

extern "C" const char* __wrap_nm_setting_ip_config_get_method(NMSettingIPConfig* setting) {
    return LibnmWraps::getInstance().nm_setting_ip_config_get_method(setting);
}

extern "C" NMSettingConnection* __wrap_nm_connection_get_setting_connection(NMConnection* connection) {
    return LibnmWraps::getInstance().nm_connection_get_setting_connection(connection);
}

extern "C" const char* __wrap_nm_setting_connection_get_interface_name(NMSettingConnection* setting) {
    return LibnmWraps::getInstance().nm_setting_connection_get_interface_name(setting);
}

extern "C" NMIPConfig* __wrap_nm_active_connection_get_ip4_config(NMActiveConnection* connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_ip4_config(connection);
}

extern "C" NMIPConfig* __wrap_nm_active_connection_get_ip6_config(NMActiveConnection* connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_ip6_config(connection);
}

extern "C" GPtrArray* __wrap_nm_ip_config_get_addresses(NMIPConfig* config) {
    return LibnmWraps::getInstance().nm_ip_config_get_addresses(config);
}

extern "C" const char* __wrap_nm_ip_address_get_address(NMIPAddress* address) {
    return LibnmWraps::getInstance().nm_ip_address_get_address(address);
}

extern "C" guint __wrap_nm_ip_address_get_prefix(NMIPAddress* address) {
    return LibnmWraps::getInstance().nm_ip_address_get_prefix(address);
}

extern "C" const char* __wrap_nm_ip_config_get_gateway(NMIPConfig* config) {
    return LibnmWraps::getInstance().nm_ip_config_get_gateway(config);
}

extern "C" const char* const* __wrap_nm_ip_config_get_nameservers(NMIPConfig* config) {
    return LibnmWraps::getInstance().nm_ip_config_get_nameservers(config);
}

extern "C" NMDhcpConfig* __wrap_nm_active_connection_get_dhcp4_config(NMActiveConnection* connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_dhcp4_config(connection);
}

extern "C" NMDhcpConfig* __wrap_nm_active_connection_get_dhcp6_config(NMActiveConnection* connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_dhcp6_config(connection);
}

extern "C" const char* __wrap_nm_dhcp_config_get_one_option(NMDhcpConfig* config, const char* option) {
    return LibnmWraps::getInstance().nm_dhcp_config_get_one_option(config, option);
}

// Access Point API wrappers
extern "C" NM80211ApFlags __wrap_nm_access_point_get_flags(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_flags(ap);
}

extern "C" NM80211ApSecurityFlags __wrap_nm_access_point_get_wpa_flags(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_wpa_flags(ap);
}

extern "C" NM80211ApSecurityFlags __wrap_nm_access_point_get_rsn_flags(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_rsn_flags(ap);
}

extern "C" GBytes* __wrap_nm_access_point_get_ssid(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_ssid(ap);
}

extern "C" const char* __wrap_nm_access_point_get_bssid(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_bssid(ap);
}

extern "C" guint32 __wrap_nm_access_point_get_frequency(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_frequency(ap);
}

extern "C" NM80211Mode __wrap_nm_access_point_get_mode(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_mode(ap);
}

extern "C" guint32 __wrap_nm_access_point_get_max_bitrate(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_max_bitrate(ap);
}

extern "C" guint8 __wrap_nm_access_point_get_strength(NMAccessPoint *ap) {
    return LibnmWraps::getInstance().nm_access_point_get_strength(ap);
}

extern "C" NMAccessPoint* __wrap_nm_device_wifi_get_active_access_point(NMDeviceWifi *device) {
    return LibnmWraps::getInstance().nm_device_wifi_get_active_access_point(device);
}

// WiFi Scan API wrappers
extern "C" void __wrap_nm_device_wifi_request_scan_async(NMDeviceWifi *device,
                                                        GCancellable *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer user_data) {
    LibnmWraps::getInstance().nm_device_wifi_request_scan_async(device, cancellable, callback, user_data);
}

extern "C" void __wrap_nm_device_wifi_request_scan_options_async(NMDeviceWifi *device,
                                                                GVariant *options,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data) {
    LibnmWraps::getInstance().nm_device_wifi_request_scan_options_async(device, options, cancellable, callback, user_data);
}

extern "C" gboolean __wrap_nm_device_wifi_request_scan_finish(NMDeviceWifi *device,
                                                             GAsyncResult *result,
                                                             GError **error) {
    return LibnmWraps::getInstance().nm_device_wifi_request_scan_finish(device, result, error);
}

LibnmWrapsImpl* LibnmWraps::impl = nullptr;
LibnmWraps::LibnmWraps() {}

void LibnmWraps::setImpl(LibnmWrapsImpl* newImpl) {
    EXPECT_TRUE((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

LibnmWraps& LibnmWraps::getInstance() {
    static LibnmWraps instance;
    return instance;
}

const char* LibnmWraps::nm_device_get_iface(NMDevice* device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_iface(device);
}

NMActiveConnection* LibnmWraps::nm_client_get_primary_connection(NMClient *client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_primary_connection(client);
}

NMRemoteConnection* LibnmWraps::nm_active_connection_get_connection(NMActiveConnection *connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_connection(connection);
}

const char* LibnmWraps::nm_connection_get_interface_name(NMRemoteConnection *connection) { // Changed to NMRemoteConnection*
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_interface_name(connection);
}

NMDevice* LibnmWraps::nm_client_get_device_by_iface(NMClient *client, const char *iface) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_device_by_iface(client, iface);
}

NMDeviceState LibnmWraps::nm_device_get_state(NMDevice *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_state(device);
}

void LibnmWraps::nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_device_disconnect_async(device, cancellable, callback, user_data);
}

gboolean LibnmWraps::nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_disconnect_finish(device, result, error);
}

const GPtrArray* LibnmWraps::nm_client_get_devices(NMClient* client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_devices(client);
}

const GPtrArray* LibnmWraps::nm_client_get_active_connections(NMClient* client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_active_connections(client);
}

NMClient* LibnmWraps::nm_client_new(GCancellable* cancellable, GError** error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_new(cancellable, error);
}

const char* LibnmWraps::nm_device_get_hw_address(NMDevice* device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_hw_address(device);
}

NMSettingIPConfig* LibnmWraps::nm_connection_get_setting_ip4_config(NMConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_setting_ip4_config(connection);
}

NMSettingIPConfig* LibnmWraps::nm_connection_get_setting_ip6_config(NMConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_setting_ip6_config(connection);
}

const char* LibnmWraps::nm_setting_ip_config_get_method(NMSettingIPConfig* setting) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_setting_ip_config_get_method(setting);
}

NMSettingConnection* LibnmWraps::nm_connection_get_setting_connection(NMConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_setting_connection(connection);
}

const char* LibnmWraps::nm_setting_connection_get_interface_name(NMSettingConnection* setting) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_setting_connection_get_interface_name(setting);
}

NMIPConfig* LibnmWraps::nm_active_connection_get_ip4_config(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_ip4_config(connection);
}

NMIPConfig* LibnmWraps::nm_active_connection_get_ip6_config(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_ip6_config(connection);
}

GPtrArray* LibnmWraps::nm_ip_config_get_addresses(NMIPConfig* config) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_config_get_addresses(config);
}

const char* LibnmWraps::nm_ip_address_get_address(NMIPAddress* address) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_address_get_address(address);
}

guint LibnmWraps::nm_ip_address_get_prefix(NMIPAddress* address) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_address_get_prefix(address);
}

const char* LibnmWraps::nm_ip_config_get_gateway(NMIPConfig* config) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_config_get_gateway(config);
}

const char* const* LibnmWraps::nm_ip_config_get_nameservers(NMIPConfig* config) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_config_get_nameservers(config);
}

NMDhcpConfig* LibnmWraps::nm_active_connection_get_dhcp4_config(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_dhcp4_config(connection);
}

NMDhcpConfig* LibnmWraps::nm_active_connection_get_dhcp6_config(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_dhcp6_config(connection);
}

const char* LibnmWraps::nm_dhcp_config_get_one_option(NMDhcpConfig* config, const char* option) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_dhcp_config_get_one_option(config, option);
}

// Access Point API implementation
NM80211ApFlags LibnmWraps::nm_access_point_get_flags(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_flags(ap);
}

NM80211ApSecurityFlags LibnmWraps::nm_access_point_get_wpa_flags(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_wpa_flags(ap);
}

NM80211ApSecurityFlags LibnmWraps::nm_access_point_get_rsn_flags(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_rsn_flags(ap);
}

GBytes* LibnmWraps::nm_access_point_get_ssid(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_ssid(ap);
}

const char* LibnmWraps::nm_access_point_get_bssid(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_bssid(ap);
}

guint32 LibnmWraps::nm_access_point_get_frequency(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_frequency(ap);
}

NM80211Mode LibnmWraps::nm_access_point_get_mode(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_mode(ap);
}

guint32 LibnmWraps::nm_access_point_get_max_bitrate(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_max_bitrate(ap);
}

guint8 LibnmWraps::nm_access_point_get_strength(NMAccessPoint *ap) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_access_point_get_strength(ap);
}

NMAccessPoint* LibnmWraps::nm_device_wifi_get_active_access_point(NMDeviceWifi *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_wifi_get_active_access_point(device);
}

// WiFi Scan API implementation
void LibnmWraps::nm_device_wifi_request_scan_async(NMDeviceWifi *device,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_device_wifi_request_scan_async(device, cancellable, callback, user_data);
}

void LibnmWraps::nm_device_wifi_request_scan_options_async(NMDeviceWifi *device,
                                                          GVariant *options,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_device_wifi_request_scan_options_async(device, options, cancellable, callback, user_data);
}

gboolean LibnmWraps::nm_device_wifi_request_scan_finish(NMDeviceWifi *device,
                                                       GAsyncResult *result,
                                                       GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_wifi_request_scan_finish(device, result, error);
}
