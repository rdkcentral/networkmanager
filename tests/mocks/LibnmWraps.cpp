#include "LibnmWraps.h"
#include <gmock/gmock.h>
#include <glib.h>

extern "C" const GPtrArray* __real_nm_client_get_devices(NMClient* client);

extern "C" NMClient* __real_nm_client_new(GCancellable* cancellable, GError** error);

extern "C" gint64 __wrap_nm_device_wifi_get_last_scan(NMDeviceWifi *device) {
    return LibnmWraps::getInstance().nm_device_wifi_get_last_scan(device);
}

extern "C" NMState __wrap_nm_client_get_state(NMClient *client) {
    return LibnmWraps::getInstance().nm_client_get_state(client);
}

extern "C" gboolean __wrap_nm_client_get_nm_running(NMClient *client) {
    return LibnmWraps::getInstance().nm_client_get_nm_running(client);
}
extern "C" void __wrap_nm_device_set_autoconnect(NMDevice *device, gboolean autoconnect) {
    LibnmWraps::getInstance().nm_device_set_autoconnect(device, autoconnect);
}
extern "C" const GPtrArray* __wrap_nm_client_get_connections(NMClient *client) {
    return LibnmWraps::getInstance().nm_client_get_connections(client);
}
extern "C" const char* __wrap_nm_connection_get_id(NMConnection *connection) {
    return LibnmWraps::getInstance().nm_connection_get_id(connection);
}
extern "C" const char* __wrap_nm_connection_get_connection_type(NMConnection *connection) {
    return LibnmWraps::getInstance().nm_connection_get_connection_type(connection);
}

extern "C" void __wrap_nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    LibnmWraps::getInstance().nm_client_activate_connection_async(client, connection, device, specific_object, cancellable, callback, user_data);
}

extern "C" NMActiveConnection* __wrap_nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error) {
    return LibnmWraps::getInstance().nm_client_activate_connection_finish(client, result, error);
}

extern "C" void __wrap_nm_client_add_and_activate_connection_async(NMClient *client, NMConnection *partial, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    LibnmWraps::getInstance().nm_client_add_and_activate_connection_async(client, partial, device, specific_object, cancellable, callback, user_data);
}

extern "C" NMActiveConnection* __wrap_nm_client_add_and_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error) {
    return LibnmWraps::getInstance().nm_client_add_and_activate_connection_finish(client, result, error);
}

extern "C" GVariant* __wrap_nm_remote_connection_update2_finish(NMRemoteConnection *connection, GAsyncResult *result, GError **error) {
    return LibnmWraps::getInstance().nm_remote_connection_update2_finish(connection, result, error);
}

extern "C" void __wrap_nm_client_add_connection2(NMClient *client, GVariant *settings, NMSettingsAddConnection2Flags flags, GVariant *args, gboolean ignore_out_result, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    LibnmWraps::getInstance().nm_client_add_connection2(client, settings, flags, args, ignore_out_result, cancellable, callback, user_data);
}

extern "C" NMRemoteConnection* __wrap_nm_client_add_connection2_finish(NMClient *client, GAsyncResult *result, GVariant **out_result, GError **error) {
    return LibnmWraps::getInstance().nm_client_add_connection2_finish(client, result, out_result, error);
}

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

extern "C" NMActiveConnection* __wrap_nm_device_get_active_connection(NMDevice *device) {
    return LibnmWraps::getInstance().nm_device_get_active_connection(device);
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

extern "C" const GPtrArray* __wrap_nm_device_wifi_get_access_points(NMDeviceWifi *device) {
    return LibnmWraps::getInstance().nm_device_wifi_get_access_points(device);
}

extern "C" const GPtrArray* __wrap_nm_device_get_available_connections(NMDevice *device) {
    return LibnmWraps::getInstance().nm_device_get_available_connections(device);
}

extern "C" const char* __wrap_nm_object_get_path(NMObject *object) {
    return LibnmWraps::getInstance().nm_object_get_path(object);
}

extern "C" void __wrap_nm_client_dbus_set_property(NMClient *client,
                                                  const char *object_path,
                                                  const char *interface_name,
                                                  const char *property_name,
                                                  GVariant *value,
                                                  int timeout_msec,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data) {
    LibnmWraps::getInstance().nm_client_dbus_set_property(client, object_path, interface_name, property_name, value, timeout_msec, cancellable, callback, user_data);
}

extern "C" gboolean __wrap_nm_client_dbus_set_property_finish(NMClient *client,
                                                             GAsyncResult *result,
                                                             GError **error) {
    return LibnmWraps::getInstance().nm_client_dbus_set_property_finish(client, result, error);
}

// New wrapper functions for commit changes and DHCP hostname
extern "C" gboolean __wrap_nm_remote_connection_commit_changes(NMRemoteConnection *connection,
                                                            gboolean save_to_disk,
                                                            GCancellable *cancellable,
                                                            GError **error) {
    return LibnmWraps::getInstance().nm_remote_connection_commit_changes(connection, save_to_disk, cancellable, error);
}

extern "C" const char* __wrap_nm_setting_ip_config_get_dhcp_hostname(NMSettingIPConfig *setting) {
    return LibnmWraps::getInstance().nm_setting_ip_config_get_dhcp_hostname(setting);
}

extern "C" gboolean __wrap_nm_setting_ip_config_get_dhcp_send_hostname(NMSettingIPConfig *setting) {
    return LibnmWraps::getInstance().nm_setting_ip_config_get_dhcp_send_hostname(setting);
}

extern "C" void __wrap_nm_connection_add_setting(NMConnection *connection, NMSetting *setting) {
    LibnmWraps::getInstance().nm_connection_add_setting(connection, setting);
}

extern "C" NMSettingWireless* __wrap_nm_connection_get_setting_wireless(NMConnection *connection) {
    return LibnmWraps::getInstance().nm_connection_get_setting_wireless(connection);
}

extern "C" GBytes* __wrap_nm_setting_wireless_get_ssid(NMSettingWireless *setting) {
    return LibnmWraps::getInstance().nm_setting_wireless_get_ssid(setting);
}

extern "C" gboolean __wrap_nm_remote_connection_delete(NMRemoteConnection *connection, GCancellable *cancellable, GError **error) {
    return LibnmWraps::getInstance().nm_remote_connection_delete(connection, cancellable, error);
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


extern "C" const char* __wrap_nm_active_connection_get_id(NMActiveConnection *connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_id(connection);
}

extern "C" const char* __wrap_nm_active_connection_get_connection_type(NMActiveConnection *connection) {
    return LibnmWraps::getInstance().nm_active_connection_get_connection_type(connection);
}

extern "C" NMDeviceStateReason __wrap_nm_device_get_state_reason(NMDevice *device) {
    return LibnmWraps::getInstance().nm_device_get_state_reason(device);
}

extern "C" int __wrap_nm_ip_address_get_family(NMIPAddress *address) {
    return LibnmWraps::getInstance().nm_ip_address_get_family(address);
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

NMActiveConnection* LibnmWraps::nm_device_get_active_connection(NMDevice *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_active_connection(device);
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
    if (impl == nullptr) {
        // During cleanup, impl may be nullptr, so call the real function directly
        return __real_nm_client_get_devices(client);
    }
    return impl->nm_client_get_devices(client);
}

const GPtrArray* LibnmWraps::nm_client_get_active_connections(NMClient* client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_active_connections(client);
}

NMClient* LibnmWraps::nm_client_new(GCancellable* cancellable, GError** error) {
    if (impl == nullptr) {
        // During cleanup, impl may be nullptr, so call the real function directly
        return __real_nm_client_new(cancellable, error);
    }
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

const GPtrArray* LibnmWraps::nm_device_wifi_get_access_points(NMDeviceWifi *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_wifi_get_access_points(device);
}

const GPtrArray* LibnmWraps::nm_device_get_available_connections(NMDevice *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_available_connections(device);
}

const char* LibnmWraps::nm_object_get_path(NMObject *object) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_object_get_path(object);
}

void LibnmWraps::nm_client_dbus_set_property(NMClient *client,
                                           const char *object_path,
                                           const char *interface_name,
                                           const char *property_name,
                                           GVariant *value,
                                           int timeout_msec,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_client_dbus_set_property(client, object_path, interface_name, property_name, value, timeout_msec, cancellable, callback, user_data);
}

gboolean LibnmWraps::nm_client_dbus_set_property_finish(NMClient *client,
                                                      GAsyncResult *result,
                                                      GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_dbus_set_property_finish(client, result, error);
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

gint64 LibnmWraps::nm_device_wifi_get_last_scan(NMDeviceWifi *device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_wifi_get_last_scan(device);
}
void LibnmWraps::nm_device_set_autoconnect(NMDevice *device, gboolean autoconnect) {
    EXPECT_NE(impl, nullptr);
    impl->nm_device_set_autoconnect(device, autoconnect);
}
const GPtrArray* LibnmWraps::nm_client_get_connections(NMClient *client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_connections(client);
}
const char* LibnmWraps::nm_connection_get_id(NMConnection *connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_id(connection);
}
const char* LibnmWraps::nm_connection_get_connection_type(NMConnection *connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_connection_type(connection);
}

void LibnmWraps::nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_client_activate_connection_async(client, connection, device, specific_object, cancellable, callback, user_data);
}

NMActiveConnection* LibnmWraps::nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_activate_connection_finish(client, result, error);
}

void LibnmWraps::nm_client_add_and_activate_connection_async(NMClient *client, NMConnection *partial, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_client_add_and_activate_connection_async(client, partial, device, specific_object, cancellable, callback, user_data);
}

NMActiveConnection* LibnmWraps::nm_client_add_and_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_add_and_activate_connection_finish(client, result, error);
}

GVariant* LibnmWraps::nm_remote_connection_update2_finish(NMRemoteConnection *connection, GAsyncResult *result, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_remote_connection_update2_finish(connection, result, error);
}

void LibnmWraps::nm_client_add_connection2(NMClient *client, GVariant *settings, NMSettingsAddConnection2Flags flags, GVariant *args, gboolean ignore_out_result, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    EXPECT_NE(impl, nullptr);
    impl->nm_client_add_connection2(client, settings, flags, args, ignore_out_result, cancellable, callback, user_data);
}

NMRemoteConnection* LibnmWraps::nm_client_add_connection2_finish(NMClient *client, GAsyncResult *result, GVariant **out_result, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_add_connection2_finish(client, result, out_result, error);
}

// New commit and DHCP hostname implementation
gboolean LibnmWraps::nm_remote_connection_commit_changes(NMRemoteConnection *connection, gboolean save_to_disk, GCancellable *cancellable, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_remote_connection_commit_changes(connection, save_to_disk, cancellable, error);
}

const char* LibnmWraps::nm_setting_ip_config_get_dhcp_hostname(NMSettingIPConfig *setting) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_setting_ip_config_get_dhcp_hostname(setting);
}

gboolean LibnmWraps::nm_setting_ip_config_get_dhcp_send_hostname(NMSettingIPConfig *setting) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_setting_ip_config_get_dhcp_send_hostname(setting);
}

void LibnmWraps::nm_connection_add_setting(NMConnection *connection, NMSetting *setting) {
    EXPECT_NE(impl, nullptr);
    impl->nm_connection_add_setting(connection, setting);
}

NMSettingWireless* LibnmWraps::nm_connection_get_setting_wireless(NMConnection *connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_connection_get_setting_wireless(connection);
}

GBytes* LibnmWraps::nm_setting_wireless_get_ssid(NMSettingWireless *setting) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_setting_wireless_get_ssid(setting);
}

gboolean LibnmWraps::nm_remote_connection_delete(NMRemoteConnection *connection, GCancellable *cancellable, GError **error) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_remote_connection_delete(connection, cancellable, error);
}

NMState LibnmWraps::nm_client_get_state(NMClient *client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_state(client);
}

gboolean LibnmWraps::nm_client_get_nm_running(NMClient *client) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_client_get_nm_running(client);
}

const char* LibnmWraps::nm_active_connection_get_id(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_id(connection);
}

const char* LibnmWraps::nm_active_connection_get_connection_type(NMActiveConnection* connection) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_active_connection_get_connection_type(connection);
}

NMDeviceStateReason LibnmWraps::nm_device_get_state_reason(NMDevice* device) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_device_get_state_reason(device);
}

int LibnmWraps::nm_ip_address_get_family(NMIPAddress* address) {
    EXPECT_NE(impl, nullptr);
    return impl->nm_ip_address_get_family(address);
}
