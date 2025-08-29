#pragma once

#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include <glib.h>
#include <string>

class LibnmWrapsImpl {
public:
    virtual ~LibnmWrapsImpl() = default;

    // New APIs
    virtual gint64 nm_device_wifi_get_last_scan(NMDeviceWifi *device) = 0;
    virtual void nm_device_set_autoconnect(NMDevice *device, gboolean autoconnect) = 0;
    virtual const GPtrArray* nm_client_get_connections(NMClient *client) = 0;
    virtual const char* nm_connection_get_id(NMConnection *connection) = 0;
    virtual const char* nm_connection_get_connection_type(NMConnection *connection) = 0;
    virtual const char* nm_device_get_iface(NMDevice* device) = 0;
    virtual NMDevice* nm_client_get_device_by_iface(NMClient *client, const char *iface) = 0;
    virtual NMDeviceState nm_device_get_state(NMDevice *device) = 0;
    virtual void nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) = 0;
    virtual gboolean nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error) = 0;
    virtual NMActiveConnection* nm_client_get_primary_connection(NMClient *client) = 0;
    virtual NMActiveConnection* nm_device_get_active_connection(NMDevice *device) = 0;
    virtual NMRemoteConnection* nm_active_connection_get_connection(NMActiveConnection *connection) = 0;
    virtual const char* nm_connection_get_interface_name(NMRemoteConnection *connection) = 0;
    virtual NMClient* nm_client_new(GCancellable* cancellable, GError** error) = 0;
    virtual const GPtrArray* nm_client_get_devices(NMClient* client) = 0;
    virtual const char* nm_device_get_hw_address(NMDevice* device) = 0;
    virtual const GPtrArray* nm_client_get_active_connections(NMClient* client) = 0;
    virtual NMSettingIPConfig* nm_connection_get_setting_ip4_config(NMConnection* connection) = 0;
    virtual NMSettingIPConfig* nm_connection_get_setting_ip6_config(NMConnection* connection) = 0;
    virtual const char* nm_setting_ip_config_get_method(NMSettingIPConfig* setting) = 0;
    virtual NMSettingConnection* nm_connection_get_setting_connection(NMConnection* connection) = 0;
    virtual const char* nm_setting_connection_get_interface_name(NMSettingConnection* setting) = 0;
    virtual NMIPConfig* nm_active_connection_get_ip4_config(NMActiveConnection* connection) = 0;
    virtual NMIPConfig* nm_active_connection_get_ip6_config(NMActiveConnection* connection) = 0;
    virtual GPtrArray* nm_ip_config_get_addresses(NMIPConfig* config) = 0;
    virtual const char* nm_ip_address_get_address(NMIPAddress* address) = 0;
    virtual guint nm_ip_address_get_prefix(NMIPAddress* address) = 0;
    virtual const char* nm_ip_config_get_gateway(NMIPConfig* config) = 0;
    virtual const char* const* nm_ip_config_get_nameservers(NMIPConfig* config) = 0;
    virtual NMDhcpConfig* nm_active_connection_get_dhcp4_config(NMActiveConnection* connection) = 0;
    virtual NMDhcpConfig* nm_active_connection_get_dhcp6_config(NMActiveConnection* connection) = 0;
    virtual const char* nm_dhcp_config_get_one_option(NMDhcpConfig* config, const char* option) = 0;
    
    // Access Point APIs
    virtual NM80211ApFlags nm_access_point_get_flags(NMAccessPoint *ap) = 0;
    virtual NM80211ApSecurityFlags nm_access_point_get_wpa_flags(NMAccessPoint *ap) = 0;
    virtual NM80211ApSecurityFlags nm_access_point_get_rsn_flags(NMAccessPoint *ap) = 0;
    virtual GBytes* nm_access_point_get_ssid(NMAccessPoint *ap) = 0;
    virtual const char* nm_access_point_get_bssid(NMAccessPoint *ap) = 0;
    virtual guint32 nm_access_point_get_frequency(NMAccessPoint *ap) = 0;
    virtual NM80211Mode nm_access_point_get_mode(NMAccessPoint *ap) = 0;
    virtual guint32 nm_access_point_get_max_bitrate(NMAccessPoint *ap) = 0;
    virtual guint8 nm_access_point_get_strength(NMAccessPoint *ap) = 0;
    virtual NMAccessPoint* nm_device_wifi_get_active_access_point(NMDeviceWifi *device) = 0;
    virtual const GPtrArray* nm_device_wifi_get_access_points(NMDeviceWifi *device) = 0;
    virtual const GPtrArray* nm_device_get_available_connections(NMDevice* device) = 0;
    
    // WiFi Scan APIs
    virtual void nm_device_wifi_request_scan_async(NMDeviceWifi *device,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data) = 0;
    virtual void nm_device_wifi_request_scan_options_async(NMDeviceWifi *device,
                                                          GVariant *options,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data) = 0;
    virtual gboolean nm_device_wifi_request_scan_finish(NMDeviceWifi *device,
                                                       GAsyncResult *result,
                                                       GError **error) = 0;
    virtual void nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device, 
                                                    const char *specific_object, GCancellable *cancellable, 
                                                    GAsyncReadyCallback callback, gpointer user_data) = 0;
    virtual NMActiveConnection *nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error) = 0;
    virtual void nm_client_add_and_activate_connection_async(NMClient *client,
                                                            NMConnection *partial,
                                                            NMDevice *device,
                                                            const char *specific_object,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data) = 0;
    virtual NMActiveConnection *nm_client_add_and_activate_connection_finish(NMClient *client,
                                                                            GAsyncResult *result,
                                                                            GError **error) = 0;
    virtual GVariant *nm_remote_connection_update2_finish(NMRemoteConnection *connection,
                                                        GAsyncResult *result,
                                                        GError **error) = 0;
    virtual void nm_client_add_connection2(NMClient *client,
                                        GVariant *settings,
                                        NMSettingsAddConnection2Flags flags,
                                        GVariant *args,
                                        gboolean ignore_out_result,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data) = 0;
    virtual NMRemoteConnection *nm_client_add_connection2_finish(NMClient *client,
                                                                GAsyncResult *result,
                                                                GVariant **out_result,
                                                                GError **error) = 0;
    virtual const char *nm_object_get_path(NMObject *object) = 0;
    virtual void nm_client_dbus_set_property(NMClient *client,
                                           const char *object_path,
                                           const char *interface_name,
                                           const char *property_name,
                                           GVariant *value,
                                           int timeout_msec,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data) = 0;
    virtual gboolean nm_client_dbus_set_property_finish(NMClient *client,
                                                      GAsyncResult *result,
                                                      GError **error) = 0;
                                                      
    // New commit and DHCP hostname functions
    virtual gboolean nm_remote_connection_commit_changes(NMRemoteConnection *connection,
                                                        gboolean save_to_disk,
                                                        GCancellable *cancellable,
                                                        GError **error) = 0;
    virtual const char *nm_setting_ip_config_get_dhcp_hostname(NMSettingIPConfig *setting) = 0;
    virtual gboolean nm_setting_ip_config_get_dhcp_send_hostname(NMSettingIPConfig *setting) = 0;
    virtual void nm_connection_add_setting(NMConnection *connection, NMSetting *setting) = 0;
    virtual NMSettingWireless *nm_connection_get_setting_wireless(NMConnection *connection) = 0;
    virtual GBytes *nm_setting_wireless_get_ssid(NMSettingWireless *setting) = 0;
    virtual gboolean nm_remote_connection_delete(NMRemoteConnection *connection, GCancellable *cancellable, GError **error) = 0;
    virtual NMState nm_client_get_state(NMClient *client) = 0;
    virtual gboolean nm_client_get_nm_running(NMClient *client) = 0;
    virtual const char* nm_active_connection_get_id(NMActiveConnection *connection) = 0;
    virtual const char* nm_active_connection_get_connection_type(NMActiveConnection *connection) = 0;
    virtual NMDeviceStateReason nm_device_get_state_reason(NMDevice *device) = 0;
    virtual int nm_ip_address_get_family(NMIPAddress *address) = 0;
  

};

class LibnmWraps {
protected:
    static LibnmWrapsImpl* impl;

public:
    LibnmWraps();
    LibnmWraps(const LibnmWraps &obj) = delete;
    static void setImpl(LibnmWrapsImpl* newImpl);
    static LibnmWraps& getInstance();

    static const char* nm_active_connection_get_id(NMActiveConnection *connection);
    static const char* nm_active_connection_get_connection_type(NMActiveConnection *connection);
    static NMDeviceStateReason nm_device_get_state_reason(NMDevice *device);
    static int nm_ip_address_get_family(NMIPAddress *address);
    static NMState nm_client_get_state(NMClient *client);
    static gboolean nm_client_get_nm_running(NMClient *client);
    static gint64 nm_device_wifi_get_last_scan(NMDeviceWifi *device);
    static void nm_device_set_autoconnect(NMDevice *device, gboolean autoconnect);
    static const GPtrArray* nm_client_get_connections(NMClient *client);
    static const char* nm_connection_get_id(NMConnection *connection);
    static const char* nm_connection_get_connection_type(NMConnection *connection);
    static void nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device, const char *specific_object, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
    static NMActiveConnection *nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error);
    static void nm_client_add_and_activate_connection_async(NMClient *client,
                                                          NMConnection *partial,
                                                          NMDevice *device,
                                                          const char *specific_object,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data);
    static NMActiveConnection *nm_client_add_and_activate_connection_finish(NMClient *client,
                                                                          GAsyncResult *result,
                                                                          GError **error);
    static GVariant *nm_remote_connection_update2_finish(NMRemoteConnection *connection,
                                                       GAsyncResult *result,
                                                       GError **error);
    static void nm_client_add_connection2(NMClient *client,
                                        GVariant *settings,
                                        NMSettingsAddConnection2Flags flags,
                                        GVariant *args,
                                        gboolean ignore_out_result,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
    static NMRemoteConnection *nm_client_add_connection2_finish(NMClient *client,
                                                              GAsyncResult *result,
                                                              GVariant **out_result,
                                                              GError **error);

    // Access Point APIs
    static const char* nm_device_get_iface(NMDevice* device);
    static NMDevice* nm_client_get_device_by_iface(NMClient *client, const char *iface);
    static NMDeviceState nm_device_get_state(NMDevice *device);
    static void nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
    static gboolean nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error);
    static NMActiveConnection* nm_client_get_primary_connection(NMClient *client);
    static NMActiveConnection* nm_device_get_active_connection(NMDevice *device);
    static NMRemoteConnection* nm_active_connection_get_connection(NMActiveConnection *connection);
    static const char* nm_connection_get_interface_name(NMRemoteConnection *connection);
    static NMClient* nm_client_new(GCancellable* cancellable, GError** error);
    static const GPtrArray* nm_client_get_devices(NMClient* client);
    static const char* nm_device_get_hw_address(NMDevice* device);
    static const GPtrArray* nm_client_get_active_connections(NMClient* client);
    static NMSettingIPConfig* nm_connection_get_setting_ip4_config(NMConnection* connection);
    static NMSettingIPConfig* nm_connection_get_setting_ip6_config(NMConnection* connection);
    static const char* nm_setting_ip_config_get_method(NMSettingIPConfig* setting);
    static NMSettingConnection* nm_connection_get_setting_connection(NMConnection* connection);
    static const char* nm_setting_connection_get_interface_name(NMSettingConnection* setting);
    static NMIPConfig* nm_active_connection_get_ip4_config(NMActiveConnection* connection);
    static NMIPConfig* nm_active_connection_get_ip6_config(NMActiveConnection* connection);
    static GPtrArray* nm_ip_config_get_addresses(NMIPConfig* config);
    static const char* nm_ip_address_get_address(NMIPAddress* address);
    static guint nm_ip_address_get_prefix(NMIPAddress* address);
    static const char* nm_ip_config_get_gateway(NMIPConfig* config);
    static const char* const* nm_ip_config_get_nameservers(NMIPConfig* config);
    static NMDhcpConfig* nm_active_connection_get_dhcp4_config(NMActiveConnection* connection);
    static NMDhcpConfig* nm_active_connection_get_dhcp6_config(NMActiveConnection* connection);
    static const char* nm_dhcp_config_get_one_option(NMDhcpConfig* config, const char* option);

    // Access Point APIs
    static NM80211ApFlags nm_access_point_get_flags(NMAccessPoint *ap);
    static NM80211ApSecurityFlags nm_access_point_get_wpa_flags(NMAccessPoint *ap);
    static NM80211ApSecurityFlags nm_access_point_get_rsn_flags(NMAccessPoint *ap);
    static GBytes* nm_access_point_get_ssid(NMAccessPoint *ap);
    static const char* nm_access_point_get_bssid(NMAccessPoint *ap);
    static guint32 nm_access_point_get_frequency(NMAccessPoint *ap);
    static NM80211Mode nm_access_point_get_mode(NMAccessPoint *ap);
    static guint32 nm_access_point_get_max_bitrate(NMAccessPoint *ap);
    static guint8 nm_access_point_get_strength(NMAccessPoint *ap);
    static NMAccessPoint* nm_device_wifi_get_active_access_point(NMDeviceWifi *device);
    static const GPtrArray* nm_device_wifi_get_access_points(NMDeviceWifi *device);
    static const GPtrArray* nm_device_get_available_connections(NMDevice* device);
    
    // WiFi Scan APIs
    static void nm_device_wifi_request_scan_async(NMDeviceWifi *device,
                                                 GCancellable *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data);
    static void nm_device_wifi_request_scan_options_async(NMDeviceWifi *device,
                                                         GVariant *options,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer user_data);
    static gboolean nm_device_wifi_request_scan_finish(NMDeviceWifi *device,
                                                      GAsyncResult *result,
                                                      GError **error);
    static const char *nm_object_get_path(NMObject *object);
    static void nm_client_dbus_set_property(NMClient *client,
                                          const char *object_path,
                                          const char *interface_name,
                                          const char *property_name,
                                          GVariant *value,
                                          int timeout_msec,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data);
    static gboolean nm_client_dbus_set_property_finish(NMClient *client,
                                                     GAsyncResult *result,
                                                     GError **error);
                                                     
    // New commit and DHCP hostname functions
    static gboolean nm_remote_connection_commit_changes(NMRemoteConnection *connection,
                                                      gboolean save_to_disk,
                                                      GCancellable *cancellable,
                                                      GError **error);
    static const char *nm_setting_ip_config_get_dhcp_hostname(NMSettingIPConfig *setting);
    static gboolean nm_setting_ip_config_get_dhcp_send_hostname(NMSettingIPConfig *setting);
    static void nm_connection_add_setting(NMConnection *connection, NMSetting *setting);
    static NMSettingWireless *nm_connection_get_setting_wireless(NMConnection *connection);
    static GBytes *nm_setting_wireless_get_ssid(NMSettingWireless *setting);
    static gboolean nm_remote_connection_delete(NMRemoteConnection *connection, GCancellable *cancellable, GError **error);
};
