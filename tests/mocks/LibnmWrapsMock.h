#pragma once

#include <gmock/gmock.h>
#include <NetworkManager.h>
#include "LibnmWraps.h"
#include <glib.h>

extern "C" const char* __real_nm_active_connection_get_id(NMActiveConnection *connection);
extern "C" const char* __real_nm_active_connection_get_connection_type(NMActiveConnection *connection);
extern "C" NMDeviceStateReason __real_nm_device_get_state_reason(NMDevice *device);
extern "C" int __real_nm_ip_address_get_family(NMIPAddress *address);
extern "C" const char* __real_nm_device_get_iface(NMDevice* device);
extern "C" gboolean __real_nm_remote_connection_delete(NMRemoteConnection *connection, GCancellable *cancellable, GError **error);
extern "C" NMActiveConnection* __real_nm_client_get_primary_connection(NMClient *client);
extern "C" NMActiveConnection* __real_nm_device_get_active_connection(NMDevice *device);
extern "C" NMRemoteConnection* __real_nm_active_connection_get_connection(NMActiveConnection *connection);
extern "C" const char* __real_nm_connection_get_interface_name(NMRemoteConnection *connection);
extern "C" NMDevice* __real_nm_client_get_device_by_iface(NMClient *client, const char *iface);
extern "C" NMDeviceState __real_nm_device_get_state(NMDevice *device);
extern "C" void __real_nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
extern "C" gboolean __real_nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error);
extern "C" const GPtrArray* __real_nm_client_get_devices(NMClient* client);
extern "C" NMClient* __real_nm_client_new(GCancellable* cancellable, GError** error);
extern "C" const char* __real_nm_device_get_hw_address(NMDevice* device);
extern "C" const GPtrArray* __real_nm_client_get_active_connections(NMClient* client);
extern "C" NMSettingIPConfig* __real_nm_connection_get_setting_ip4_config(NMConnection* connection);
extern "C" NMSettingIPConfig* __real_nm_connection_get_setting_ip6_config(NMConnection* connection);
extern "C" const char* __real_nm_setting_ip_config_get_method(NMSettingIPConfig* setting);
extern "C" NMSettingConnection* __real_nm_connection_get_setting_connection(NMConnection* connection);
extern "C" const char* __real_nm_setting_connection_get_interface_name(NMSettingConnection* setting);
extern "C" NMIPConfig* __real_nm_active_connection_get_ip4_config(NMActiveConnection* connection);
extern "C" NMIPConfig* __real_nm_active_connection_get_ip6_config(NMActiveConnection* connection);
extern "C" GPtrArray* __real_nm_ip_config_get_addresses(NMIPConfig* config);
extern "C" const char* __real_nm_ip_address_get_address(NMIPAddress* address);
extern "C" guint __real_nm_ip_address_get_prefix(NMIPAddress* address);
extern "C" const char* __real_nm_ip_config_get_gateway(NMIPConfig* config);
extern "C" const char* const* __real_nm_ip_config_get_nameservers(NMIPConfig* config);
extern "C" NMDhcpConfig* __real_nm_active_connection_get_dhcp4_config(NMActiveConnection* connection);
extern "C" NMDhcpConfig* __real_nm_active_connection_get_dhcp6_config(NMActiveConnection* connection);
extern "C" const char* __real_nm_dhcp_config_get_one_option(NMDhcpConfig* config, const char* option);

extern "C" NM80211ApFlags __real_nm_access_point_get_flags(NMAccessPoint *ap);
extern "C" NM80211ApSecurityFlags __real_nm_access_point_get_wpa_flags(NMAccessPoint *ap);
extern "C" NM80211ApSecurityFlags __real_nm_access_point_get_rsn_flags(NMAccessPoint *ap);
extern "C" GBytes* __real_nm_access_point_get_ssid(NMAccessPoint *ap);
extern "C" const char* __real_nm_access_point_get_bssid(NMAccessPoint *ap);
extern "C" guint32 __real_nm_access_point_get_frequency(NMAccessPoint *ap);
extern "C" NM80211Mode __real_nm_access_point_get_mode(NMAccessPoint *ap);
extern "C" guint32 __real_nm_access_point_get_max_bitrate(NMAccessPoint *ap);
extern "C" guint8 __real_nm_access_point_get_strength(NMAccessPoint *ap);
extern "C" gboolean __real_nm_access_point_connection_valid(NMAccessPoint *ap, NMConnection *connection);
extern "C" NMAccessPoint* __real_nm_device_wifi_get_active_access_point(NMDeviceWifi *device);

extern "C" void __real_nm_device_wifi_request_scan_async(NMDeviceWifi *device,
                                                        GCancellable *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer user_data);
extern "C" void __real_nm_device_wifi_request_scan_options_async(NMDeviceWifi *device,
                                                                GVariant *options,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);
extern "C" gboolean __real_nm_device_wifi_request_scan_finish(NMDeviceWifi *device,
                                                             GAsyncResult *result,
                                                             GError **error);
extern "C" const GPtrArray *__real_nm_device_wifi_get_access_points(NMDeviceWifi *device);
extern "C" const GPtrArray *__real_nm_device_get_available_connections(NMDevice *device);

extern "C" gint64 __real_nm_device_wifi_get_last_scan(NMDeviceWifi *device);
extern "C" void __real_nm_device_set_autoconnect(NMDevice *device, gboolean autoconnect);
extern "C" const GPtrArray* __real_nm_client_get_connections(NMClient *client);
extern "C" const char* __real_nm_connection_get_id(NMConnection *connection);
extern "C" const char* __real_nm_connection_get_connection_type(NMConnection *connection);
extern "C" void __real_nm_client_activate_connection_async(NMClient *client,
                                                          NMConnection *connection,
                                                          NMDevice *device,
                                                          const char *specific_object,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data);
extern "C" NMActiveConnection* __real_nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error);
extern "C" void __real_nm_client_add_and_activate_connection_async(NMClient *client,
                                                                  NMConnection *partial,
                                                                  NMDevice *device,
                                                                  const char *specific_object,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);
extern "C" NMActiveConnection* __real_nm_client_add_and_activate_connection_finish(NMClient *client,
                                                                                 GAsyncResult *result,
                                                                                 GError **error);
extern "C" GVariant* __real_nm_remote_connection_update2_finish(NMRemoteConnection *connection,
                                                               GAsyncResult *result,
                                                               GError **error);
extern "C" void __real_nm_remote_connection_update2(NMRemoteConnection *connection,
                                                  GVariant *settings,
                                                  NMSettingsUpdate2Flags flags,
                                                  GVariant *args,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data);
extern "C" void __real_nm_client_add_connection2(NMClient *client,
                                               GVariant *settings,
                                               NMSettingsAddConnection2Flags flags,
                                               GVariant *args,
                                               gboolean ignore_out_result,
                                               GCancellable *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer user_data);
extern "C" NMRemoteConnection* __real_nm_client_add_connection2_finish(NMClient *client,
                                                                     GAsyncResult *result,
                                                                     GVariant **out_result,
                                                                     GError **error);
extern "C" const char* __real_nm_object_get_path(NMObject *object);
extern "C" void __real_nm_client_dbus_set_property(NMClient *client,
                                                  const char *object_path,
                                                  const char *interface_name,
                                                  const char *property_name,
                                                  GVariant *value,
                                                  int timeout_msec,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data);
extern "C" gboolean __real_nm_client_dbus_set_property_finish(NMClient *client,
                                                            GAsyncResult *result,
                                                            GError **error);

extern "C" gboolean __real_nm_remote_connection_commit_changes(NMRemoteConnection *connection,
                                                             gboolean save_to_disk,
                                                             GCancellable *cancellable,
                                                             GError **error);
extern "C" const char* __real_nm_setting_ip_config_get_dhcp_hostname(NMSettingIPConfig *setting);
extern "C" gboolean __real_nm_setting_ip_config_get_dhcp_send_hostname(NMSettingIPConfig *setting);
extern "C" void __real_nm_connection_add_setting(NMConnection *connection, NMSetting *setting);
extern "C" NMSettingWireless *__real_nm_connection_get_setting_wireless(NMConnection *connection);
extern "C" GBytes *__real_nm_setting_wireless_get_ssid(NMSettingWireless *setting);
extern "C" NMState __real_nm_client_get_state(NMClient *client);
extern "C" gboolean __real_nm_client_get_nm_running(NMClient *client);
extern "C" GVariant* __real_nm_connection_to_dbus(NMConnection *connection, NMConnectionSerializationFlags flags);


class LibnmWrapsImplMock : public LibnmWrapsImpl {
public:
    LibnmWrapsImplMock() : LibnmWrapsImpl() {

         ON_CALL(*this, nm_active_connection_get_id(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> const char* {
                return __real_nm_active_connection_get_id(connection);
            }));
            
        ON_CALL(*this, nm_active_connection_get_connection_type(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> const char* {
                return __real_nm_active_connection_get_connection_type(connection);
            }));
            
        ON_CALL(*this, nm_device_get_state_reason(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device) -> NMDeviceStateReason {
                return __real_nm_device_get_state_reason(device);
            }));
            
        ON_CALL(*this, nm_ip_address_get_family(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPAddress* address) -> int {
                return __real_nm_ip_address_get_family(address);
            }));
            
        ON_CALL(*this, nm_connection_to_dbus(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection, NMConnectionSerializationFlags flags) -> GVariant* {
                return __real_nm_connection_to_dbus(connection, flags);
            }));
        ON_CALL(*this, nm_device_wifi_get_last_scan(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi* device) -> gint64 {
                return __real_nm_device_wifi_get_last_scan(device);
            }));
        ON_CALL(*this, nm_device_set_autoconnect(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device, gboolean autoconnect) {
                __real_nm_device_set_autoconnect(device, autoconnect);
            }));
        ON_CALL(*this, nm_client_get_connections(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> const GPtrArray* {
                return __real_nm_client_get_connections(client);
            }));
        ON_CALL(*this, nm_connection_get_id(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection) -> const char* {
                return __real_nm_connection_get_id(connection);
            }));
        ON_CALL(*this, nm_connection_get_connection_type(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection) -> const char* {
                return __real_nm_connection_get_connection_type(connection);
            }));
        ON_CALL(*this, nm_client_activate_connection_async(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, NMConnection* connection, NMDevice* device, const char* specific_object, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_client_activate_connection_async(client, connection, device, specific_object, cancellable, callback, user_data);
            }));
        ON_CALL(*this, nm_client_activate_connection_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, GAsyncResult* result, GError** error) -> NMActiveConnection* {
                return __real_nm_client_activate_connection_finish(client, result, error);
            }));
        ON_CALL(*this, nm_client_add_and_activate_connection_async(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, NMConnection* partial, NMDevice* device, const char* specific_object, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_client_add_and_activate_connection_async(client, partial, device, specific_object, cancellable, callback, user_data);
            }));
        ON_CALL(*this, nm_client_add_and_activate_connection_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, GAsyncResult* result, GError** error) -> NMActiveConnection* {
                return __real_nm_client_add_and_activate_connection_finish(client, result, error);
            }));
        ON_CALL(*this, nm_remote_connection_update2_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMRemoteConnection* connection, GAsyncResult* result, GError** error) -> GVariant* {
                return __real_nm_remote_connection_update2_finish(connection, result, error);
            }));
            
        ON_CALL(*this, nm_remote_connection_update2(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMRemoteConnection* connection, GVariant* settings, NMSettingsUpdate2Flags flags, GVariant* args, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_remote_connection_update2(connection, settings, flags, args, cancellable, callback, user_data);
            }));
        ON_CALL(*this, nm_client_add_connection2(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, GVariant* settings, NMSettingsAddConnection2Flags flags, GVariant* args, gboolean ignore_out_result, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_client_add_connection2(client, settings, flags, args, ignore_out_result, cancellable, callback, user_data);
            }));
        ON_CALL(*this, nm_client_add_connection2_finish(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, GAsyncResult* result, GVariant** out_result, GError** error) -> NMRemoteConnection* {
                return __real_nm_client_add_connection2_finish(client, result, out_result, error);
            }));

        ON_CALL(*this, nm_device_get_iface(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device) -> const char* {
                return __real_nm_device_get_iface(device);
            }));
        ON_CALL(*this, nm_client_get_device_by_iface(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client, const char* iface) -> NMDevice* {
                return __real_nm_client_get_device_by_iface(client, iface);
            }));
        ON_CALL(*this, nm_device_get_state(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device) -> NMDeviceState {
                return __real_nm_device_get_state(device);
            }));
        ON_CALL(*this, nm_device_disconnect_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                return __real_nm_device_disconnect_async(device, cancellable, callback, user_data);
            }));
        
        ON_CALL(*this, nm_device_disconnect_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device, GAsyncResult* result, GError** error) -> gboolean {
                return __real_nm_device_disconnect_finish(device, result, error);
            }));
        ON_CALL(*this, nm_client_get_primary_connection(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> NMActiveConnection* {
                return __real_nm_client_get_primary_connection(client);
            }));
        ON_CALL(*this, nm_device_get_active_connection(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device) -> NMActiveConnection* {
                return __real_nm_device_get_active_connection(device);
            }));
        ON_CALL(*this, nm_active_connection_get_connection(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> NMRemoteConnection* {
                return __real_nm_active_connection_get_connection(connection);
            }));
        ON_CALL(*this, nm_connection_get_interface_name(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMRemoteConnection* connection) -> const char* {
                return __real_nm_connection_get_interface_name(connection);
            }));
        ON_CALL(*this, nm_client_get_devices(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> const GPtrArray* {
                return __real_nm_client_get_devices(client);
            }));
        ON_CALL(*this, nm_client_new(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](GCancellable* cancellable, GError** error) -> NMClient* {
                return __real_nm_client_new(cancellable, error);
            }));
        ON_CALL(*this, nm_device_get_hw_address(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice* device) -> const char* {
                return __real_nm_device_get_hw_address(device);
            }));
        ON_CALL(*this, nm_client_get_active_connections(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> const GPtrArray* {
                return __real_nm_client_get_active_connections(client);
            }));
        ON_CALL(*this, nm_connection_get_setting_ip4_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection) -> NMSettingIPConfig* {
                return __real_nm_connection_get_setting_ip4_config(connection);
            }));
        ON_CALL(*this, nm_connection_get_setting_ip6_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection) -> NMSettingIPConfig* {
                return __real_nm_connection_get_setting_ip6_config(connection);
            }));
        ON_CALL(*this, nm_setting_ip_config_get_method(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMSettingIPConfig* setting) -> const char* {
                return __real_nm_setting_ip_config_get_method(setting);
            }));
        ON_CALL(*this, nm_connection_get_setting_connection(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMConnection* connection) -> NMSettingConnection* {
                return __real_nm_connection_get_setting_connection(connection);
            }));
        ON_CALL(*this, nm_setting_connection_get_interface_name(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMSettingConnection* setting) -> const char* {
                return __real_nm_setting_connection_get_interface_name(setting);
            }));
        ON_CALL(*this, nm_active_connection_get_ip4_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> NMIPConfig* {
                return __real_nm_active_connection_get_ip4_config(connection);
            }));
        ON_CALL(*this, nm_active_connection_get_ip6_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> NMIPConfig* {
                return __real_nm_active_connection_get_ip6_config(connection);
            }));
        ON_CALL(*this, nm_ip_config_get_addresses(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPConfig* config) -> GPtrArray* {
                return __real_nm_ip_config_get_addresses(config);
            }));
        ON_CALL(*this, nm_ip_address_get_address(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPAddress* address) -> const char* {
                return __real_nm_ip_address_get_address(address);
            }));
        ON_CALL(*this, nm_ip_address_get_prefix(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPAddress* address) -> guint {
                return __real_nm_ip_address_get_prefix(address);
            }));
        ON_CALL(*this, nm_ip_config_get_gateway(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPConfig* config) -> const char* {
                return __real_nm_ip_config_get_gateway(config);
            }));
        ON_CALL(*this, nm_ip_config_get_nameservers(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMIPConfig* config) -> const char* const* {
                return __real_nm_ip_config_get_nameservers(config);
            }));
        ON_CALL(*this, nm_active_connection_get_dhcp4_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> NMDhcpConfig* {
                return __real_nm_active_connection_get_dhcp4_config(connection);
            }));
        ON_CALL(*this, nm_active_connection_get_dhcp6_config(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMActiveConnection* connection) -> NMDhcpConfig* {
                return __real_nm_active_connection_get_dhcp6_config(connection);
            }));
        ON_CALL(*this, nm_dhcp_config_get_one_option(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDhcpConfig* config, const char* option) -> const char* {
                return __real_nm_dhcp_config_get_one_option(config, option);
            }));
            
        // Access Point API ON_CALL setups
        ON_CALL(*this, nm_access_point_get_flags(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> NM80211ApFlags {
                return __real_nm_access_point_get_flags(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_wpa_flags(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> NM80211ApSecurityFlags {
                return __real_nm_access_point_get_wpa_flags(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_rsn_flags(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> NM80211ApSecurityFlags {
                return __real_nm_access_point_get_rsn_flags(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_ssid(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> GBytes* {
                return __real_nm_access_point_get_ssid(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_bssid(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> const char* {
                return __real_nm_access_point_get_bssid(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_frequency(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> guint32 {
                return __real_nm_access_point_get_frequency(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_mode(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> NM80211Mode {
                return __real_nm_access_point_get_mode(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_max_bitrate(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> guint32 {
                return __real_nm_access_point_get_max_bitrate(ap);
            }));
            
        ON_CALL(*this, nm_access_point_get_strength(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap) -> guint8 {
                return __real_nm_access_point_get_strength(ap);
            }));
            
        ON_CALL(*this, nm_access_point_connection_valid(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMAccessPoint* ap, NMConnection* connection) -> gboolean {
                return __real_nm_access_point_connection_valid(ap, connection);
            }));
            
        ON_CALL(*this, nm_device_wifi_get_active_access_point(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi* device) -> NMAccessPoint* {
                return __real_nm_device_wifi_get_active_access_point(device);
            }));
            
        // WiFi Scan API ON_CALL setups
        ON_CALL(*this, nm_device_wifi_request_scan_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_device_wifi_request_scan_async(device, cancellable, callback, user_data);
            }));
            
        ON_CALL(*this, nm_device_wifi_request_scan_options_async(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi *device, GVariant *options, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_device_wifi_request_scan_options_async(device, options, cancellable, callback, user_data);
            }));
            
        ON_CALL(*this, nm_device_wifi_request_scan_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi *device, GAsyncResult *result, GError **error) -> gboolean {
                return __real_nm_device_wifi_request_scan_finish(device, result, error);
            }));
        ON_CALL(*this, nm_device_wifi_get_access_points(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDeviceWifi *device) -> const GPtrArray* {
                return __real_nm_device_wifi_get_access_points(device);
            }));
        ON_CALL(*this, nm_device_get_available_connections(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMDevice *device) -> const GPtrArray* {
                return __real_nm_device_get_available_connections(device);
            }));
        ON_CALL(*this, nm_object_get_path(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMObject *object) -> const char* {
                return __real_nm_object_get_path(object);
            }));
        ON_CALL(*this, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                __real_nm_client_dbus_set_property(client, object_path, interface_name, property_name, value, timeout_msec, cancellable, callback, user_data);
            }));
        ON_CALL(*this, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
                return __real_nm_client_dbus_set_property_finish(client, result, error);
            }));
        ON_CALL(*this, nm_remote_connection_delete(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMRemoteConnection *connection, GCancellable *cancellable, GError **error) -> gboolean {
                return __real_nm_remote_connection_delete(connection, cancellable, error);
            }));
        ON_CALL(*this, nm_client_get_state(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> NMState {
                return __real_nm_client_get_state(client);
            }));
            
        ON_CALL(*this, nm_client_get_nm_running(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](NMClient* client) -> gboolean {
                return __real_nm_client_get_nm_running(client);
            }));
    }

    virtual ~LibnmWrapsImplMock() = default;

    MOCK_METHOD(const char*, nm_device_get_iface, (NMDevice* device), (override));
    MOCK_METHOD(NMDevice*, nm_client_get_device_by_iface, (NMClient *client, const char *iface), (override));
    MOCK_METHOD(NMDeviceState, nm_device_get_state, (NMDevice *device), (override));
    MOCK_METHOD(void, nm_device_disconnect_async, (NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(gboolean, nm_device_disconnect_finish, (NMDevice *device, GAsyncResult *result, GError **error), (override));
    MOCK_METHOD(NMActiveConnection*, nm_client_get_primary_connection, (NMClient *client), (override));
    MOCK_METHOD(NMActiveConnection*, nm_device_get_active_connection, (NMDevice *device), (override));
    MOCK_METHOD(NMRemoteConnection*, nm_active_connection_get_connection, (NMActiveConnection *connection), (override));
    MOCK_METHOD(const char*, nm_connection_get_interface_name, (NMRemoteConnection *connection), (override));
    MOCK_METHOD(NMClient*, nm_client_new, (GCancellable* cancellable, GError** error), (override));
    MOCK_METHOD(const GPtrArray*, nm_client_get_devices, (NMClient* client), (override));
    MOCK_METHOD(const char*, nm_device_get_hw_address, (NMDevice* device), (override));
    MOCK_METHOD(const char*, nm_setting_connection_get_interface_name, (NMSettingConnection* settings), (override));
    MOCK_METHOD(const GPtrArray*, nm_client_get_active_connections, (NMClient* client), (override));
    MOCK_METHOD(NMSettingIPConfig*, nm_connection_get_setting_ip4_config, (NMConnection* connection), (override));
    MOCK_METHOD(NMSettingIPConfig*, nm_connection_get_setting_ip6_config, (NMConnection* connection), (override));
    MOCK_METHOD(const char*, nm_setting_ip_config_get_method, (NMSettingIPConfig* setting), (override));
    MOCK_METHOD(NMSettingConnection*, nm_connection_get_setting_connection, (NMConnection* connection), (override));
    MOCK_METHOD(NMIPConfig*, nm_active_connection_get_ip4_config, (NMActiveConnection* connection), (override));
    MOCK_METHOD(NMIPConfig*, nm_active_connection_get_ip6_config, (NMActiveConnection* connection), (override));
    MOCK_METHOD(NMDhcpConfig*, nm_active_connection_get_dhcp6_config, (NMActiveConnection* connection), (override));
    MOCK_METHOD(GPtrArray*, nm_ip_config_get_addresses, (NMIPConfig* config), (override));
    MOCK_METHOD(const char*, nm_ip_address_get_address, (NMIPAddress* address), (override));
    MOCK_METHOD(guint, nm_ip_address_get_prefix, (NMIPAddress* address), (override));
    MOCK_METHOD(const char*, nm_ip_config_get_gateway, (NMIPConfig* config), (override));
    MOCK_METHOD(const char* const*, nm_ip_config_get_nameservers, (NMIPConfig* config), (override));
    MOCK_METHOD(NMDhcpConfig*, nm_active_connection_get_dhcp4_config, (NMActiveConnection* connection), (override));
    MOCK_METHOD(const char*, nm_dhcp_config_get_one_option, (NMDhcpConfig* config, const char* option), (override));

    MOCK_METHOD(NM80211ApFlags, nm_access_point_get_flags, (NMAccessPoint *ap), (override));
    MOCK_METHOD(NM80211ApSecurityFlags, nm_access_point_get_wpa_flags, (NMAccessPoint *ap), (override));
    MOCK_METHOD(NM80211ApSecurityFlags, nm_access_point_get_rsn_flags, (NMAccessPoint *ap), (override));
    MOCK_METHOD(GBytes*, nm_access_point_get_ssid, (NMAccessPoint *ap), (override));
    MOCK_METHOD(const char*, nm_access_point_get_bssid, (NMAccessPoint *ap), (override));
    MOCK_METHOD(guint32, nm_access_point_get_frequency, (NMAccessPoint *ap), (override));
    MOCK_METHOD(NM80211Mode, nm_access_point_get_mode, (NMAccessPoint *ap), (override));
    MOCK_METHOD(guint32, nm_access_point_get_max_bitrate, (NMAccessPoint *ap), (override));
    MOCK_METHOD(guint8, nm_access_point_get_strength, (NMAccessPoint *ap), (override));
    MOCK_METHOD(gboolean, nm_access_point_connection_valid, (NMAccessPoint *ap, NMConnection *connection), (override));
    MOCK_METHOD(NMAccessPoint*, nm_device_wifi_get_active_access_point, (NMDeviceWifi *device), (override));
    MOCK_METHOD(const GPtrArray*, nm_device_wifi_get_access_points, (NMDeviceWifi *device), (override));
    MOCK_METHOD(const GPtrArray*, nm_device_get_available_connections, (NMDevice *device), (override));
    MOCK_METHOD(const char*, nm_object_get_path, (NMObject *object), (override));
    MOCK_METHOD(void, nm_client_dbus_set_property, (NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(gboolean, nm_client_dbus_set_property_finish, (NMClient *client, GAsyncResult *result, GError **error), (override));

    MOCK_METHOD(void, nm_device_wifi_request_scan_async, (NMDeviceWifi *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(void, nm_device_wifi_request_scan_options_async, (NMDeviceWifi *device, GVariant *options, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(gboolean, nm_device_wifi_request_scan_finish, (NMDeviceWifi *device, GAsyncResult *result, GError **error), (override));

    MOCK_METHOD(gint64, nm_device_wifi_get_last_scan, (NMDeviceWifi *device), (override));
    MOCK_METHOD(void, nm_device_set_autoconnect, (NMDevice *device, gboolean autoconnect), (override));
    MOCK_METHOD(const GPtrArray*, nm_client_get_connections, (NMClient *client), (override));
    MOCK_METHOD(const char*, nm_connection_get_id, (NMConnection *connection), (override));
    MOCK_METHOD(const char*, nm_connection_get_connection_type, (NMConnection *connection), (override));
    MOCK_METHOD(void, nm_client_activate_connection_async, (NMClient *client, NMConnection *connection, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(NMActiveConnection*, nm_client_activate_connection_finish, (NMClient *client, GAsyncResult *result, GError **error), (override));
    MOCK_METHOD(void, nm_client_add_and_activate_connection_async, (NMClient *client, NMConnection *partial, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));

    MOCK_METHOD(gboolean, nm_remote_connection_commit_changes, (NMRemoteConnection *connection, gboolean save_to_disk, GCancellable *cancellable, GError **error), (override));
    MOCK_METHOD(const char*, nm_setting_ip_config_get_dhcp_hostname, (NMSettingIPConfig *setting), (override));
    MOCK_METHOD(gboolean, nm_setting_ip_config_get_dhcp_send_hostname, (NMSettingIPConfig *setting), (override));
    MOCK_METHOD(void, nm_connection_add_setting, (NMConnection *connection, NMSetting *setting), (override));
    MOCK_METHOD(NMSettingWireless*, nm_connection_get_setting_wireless, (NMConnection *connection), (override));
    MOCK_METHOD(GBytes*, nm_setting_wireless_get_ssid, (NMSettingWireless *setting), (override));
    MOCK_METHOD(NMActiveConnection*, nm_client_add_and_activate_connection_finish, (NMClient *client, GAsyncResult *result, GError **error), (override));
    MOCK_METHOD(GVariant*, nm_remote_connection_update2_finish, (NMRemoteConnection *connection, GAsyncResult *result, GError **error), (override));
    MOCK_METHOD(void, nm_remote_connection_update2, (NMRemoteConnection *connection, GVariant *settings, NMSettingsUpdate2Flags flags, GVariant *args, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(void, nm_client_add_connection2, (NMClient *client, GVariant *settings, NMSettingsAddConnection2Flags flags, GVariant *args, gboolean ignore_out_result, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data), (override));
    MOCK_METHOD(NMRemoteConnection*, nm_client_add_connection2_finish, (NMClient *client, GAsyncResult *result, GVariant **out_result, GError **error), (override));
    MOCK_METHOD(gboolean, nm_remote_connection_delete, (NMRemoteConnection *connection, GCancellable *cancellable, GError **error), (override));
    MOCK_METHOD(NMState, nm_client_get_state, (NMClient *client), (override));
    MOCK_METHOD(gboolean, nm_client_get_nm_running, (NMClient *client), (override));
    MOCK_METHOD(const char*, nm_active_connection_get_id, (NMActiveConnection *connection), (override));
    MOCK_METHOD(const char*, nm_active_connection_get_connection_type, (NMActiveConnection *connection), (override));
    MOCK_METHOD(NMDeviceStateReason, nm_device_get_state_reason, (NMDevice *device), (override));
    MOCK_METHOD(int, nm_ip_address_get_family, (NMIPAddress *address), (override));
    MOCK_METHOD(GVariant*, nm_connection_to_dbus, (NMConnection *connection, NMConnectionSerializationFlags flags), (override));
};
