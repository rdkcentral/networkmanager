/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <glib.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerGnomeEthWifiSwitchTest.h"

namespace WPEFramework
{
    namespace Plugin
    {
        namespace
        {
            constexpr gint64 kEthMetricWifiPreferred = 20100;  // Ethernet 100 + 20000, above Wi-Fi 600
            constexpr guint kNmCallTimeoutMs = 10000;
            constexpr unsigned long kArpFlagComplete = 0x2;    // ATF_COM
            constexpr const char* kProcNetArp = "/proc/net/arp";

            /*
             * Runs one async libnm call synchronously on the worker's private GMainContext:
             * start the call, run the loop until the callback quits it (or the timeout cancels it).
             */
            class NmCall
            {
            public:
                explicit NmCall(GMainContext* ctx)
                    : m_loop(g_main_loop_new(ctx, FALSE)), m_cancellable(g_cancellable_new()) {}
                ~NmCall()
                {
                    if (m_conn != nullptr)
                        g_object_unref(m_conn);
                    g_object_unref(m_cancellable);
                    g_main_loop_unref(m_loop);
                }
                NmCall(const NmCall&) = delete;
                NmCall& operator=(const NmCall&) = delete;

                bool getAppliedConnection(NMDevice* dev)
                {
                    nm_device_get_applied_connection_async(dev, 0, m_cancellable, appliedConnectionCb, this);
                    return wait();
                }

                bool reapply(NMDevice* dev, NMConnection* conn, guint64 versionId)
                {
                    nm_device_reapply_async(dev, conn, versionId, 0, m_cancellable, reapplyCb, this);
                    return wait();
                }

                NMConnection* connection() const { return m_conn; }
                guint64 versionId() const { return m_versionId; }

            private:
                static void appliedConnectionCb(GObject* src, GAsyncResult* res, gpointer data)
                {
                    NmCall* call = static_cast<NmCall*>(data);
                    GError* error = nullptr;
                    call->m_conn = nm_device_get_applied_connection_finish(NM_DEVICE(src), res, &call->m_versionId, &error);
                    call->m_ok = (call->m_conn != nullptr);
                    call->finish(error);
                }

                static void reapplyCb(GObject* src, GAsyncResult* res, gpointer data)
                {
                    NmCall* call = static_cast<NmCall*>(data);
                    GError* error = nullptr;
                    call->m_ok = nm_device_reapply_finish(NM_DEVICE(src), res, &error);
                    call->finish(error);
                }

                static gboolean timeoutCb(gpointer data)
                {
                    NMLOG_WARNING("NetworkManager call timed out, cancelling");
                    g_cancellable_cancel(G_CANCELLABLE(data));
                    return G_SOURCE_REMOVE;
                }

                // libnm always calls the callback, also after a cancel, so the loop ends there.
                bool wait()
                {
                    GSource* timeout = g_timeout_source_new(kNmCallTimeoutMs);
                    g_source_set_callback(timeout, timeoutCb, m_cancellable, nullptr);
                    g_source_attach(timeout, g_main_loop_get_context(m_loop));
                    if (!m_done)
                        g_main_loop_run(m_loop);
                    g_source_destroy(timeout);
                    g_source_unref(timeout);
                    return m_ok;
                }

                void finish(GError* error)
                {
                    if (error) {
                        NMLOG_ERROR("NetworkManager call failed: %s", error->message);
                        g_error_free(error);
                    }
                    m_done = true;
                    g_main_loop_quit(m_loop);
                }

                GMainLoop* m_loop;
                GCancellable* m_cancellable;
                bool m_done = false;
                bool m_ok = false;
                NMConnection* m_conn = nullptr;
                guint64 m_versionId = 0;
            };

            bool isActivated(NMDevice* dev)
            {
                return dev != nullptr && nm_device_get_state(dev) == NM_DEVICE_STATE_ACTIVATED;
            }

            // /proc/net/arp columns: IP address, HW type, Flags, HW address, Mask, Device
            bool isGatewayInArpCache(const std::string& gateway, const std::string& iface)
            {
                std::ifstream arp(kProcNetArp);
                if (!arp.is_open()) {
                    NMLOG_ERROR("cannot open %s", kProcNetArp);
                    return false;
                }

                std::string line;
                std::getline(arp, line);
                while (std::getline(arp, line)) {
                    std::istringstream fields(line);
                    std::string ip, hwType, flags, mac, mask, dev;
                    if (!(fields >> ip >> hwType >> flags >> mac >> mask >> dev))
                        continue;
                    if (ip != gateway || dev != iface)
                        continue;

                    const bool complete = (strtoul(flags.c_str(), nullptr, 16) & kArpFlagComplete) &&
                                          mac != "00:00:00:00:00:00";
                    NMLOG_INFO("ARP entry %s dev %s mac %s flags %s -> %s", ip.c_str(), dev.c_str(),
                               mac.c_str(), flags.c_str(), complete ? "reachable" : "not reachable");
                    return complete;
                }
                NMLOG_INFO("no ARP entry for gateway %s on %s", gateway.c_str(), iface.c_str());
                return false;
            }

            bool isWifiGatewayReachable(NMDevice* wlanDev, const std::string& wlanIface)
            {
                NMIPConfig* ip4 = nm_device_get_ip4_config(wlanDev);
                const char* gateway = ip4 ? nm_ip_config_get_gateway(ip4) : nullptr;
                if (gateway == nullptr || *gateway == '\0') {
                    NMLOG_INFO("no IPv4 gateway on %s", wlanIface.c_str());
                    return false;
                }
                return isGatewayInArpCache(gateway, wlanIface);
            }

            void trySwitchToWifi(NMClient* client, GMainContext* ctx, const std::string& ethIface, const std::string& wlanIface)
            {
                NMDevice* ethDev = nm_client_get_device_by_iface(client, ethIface.c_str());
                NMDevice* wlanDev = nm_client_get_device_by_iface(client, wlanIface.c_str());
                if (!isActivated(ethDev) || !isActivated(wlanDev)) {
                    NMLOG_INFO("%s or %s not activated, no switch", ethIface.c_str(), wlanIface.c_str());
                    return;
                }

                NmCall get(ctx);
                if (!get.getAppliedConnection(ethDev))
                    return;

                NMConnection* conn = get.connection();
                NMSettingIPConfig* ip4 = nm_connection_get_setting_ip4_config(conn);
                NMSettingIPConfig* ip6 = nm_connection_get_setting_ip6_config(conn);
                if (ip4 != nullptr && nm_setting_ip_config_get_route_metric(ip4) == kEthMetricWifiPreferred) {
                    NMLOG_INFO("%s route-metric already %" G_GINT64_FORMAT ", Wi-Fi already preferred",
                               ethIface.c_str(), kEthMetricWifiPreferred);
                    return;
                }
                if (!isWifiGatewayReachable(wlanDev, wlanIface)) {
                    NMLOG_INFO("%s gateway not reachable, staying on %s", wlanIface.c_str(), ethIface.c_str());
                    return;
                }

                if (ip4 != nullptr)
                    g_object_set(ip4, NM_SETTING_IP_CONFIG_ROUTE_METRIC, kEthMetricWifiPreferred, nullptr);
                if (ip6 != nullptr)
                    g_object_set(ip6, NM_SETTING_IP_CONFIG_ROUTE_METRIC, kEthMetricWifiPreferred, nullptr);

                NmCall reapply(ctx);
                if (reapply.reapply(ethDev, conn, get.versionId()))
                    NMLOG_INFO("switched to %s: %s route-metric %" G_GINT64_FORMAT " (in memory)",
                               wlanIface.c_str(), ethIface.c_str(), kEthMetricWifiPreferred);
                else
                    NMLOG_ERROR("Reapply on %s failed, no switch", ethIface.c_str());
            }

            void destroyClient(NMClient* client, GMainContext* ctx)
            {
                GObject* busyWatcher = nm_client_get_context_busy_watcher(client);
                g_object_add_weak_pointer(busyWatcher, reinterpret_cast<gpointer*>(&busyWatcher));
                g_object_unref(client);
                while (busyWatcher != nullptr)
                    g_main_context_iteration(ctx, TRUE);
            }
        }

        EthWifiSwitchTest& EthWifiSwitchTest::getInstance()
        {
            static EthWifiSwitchTest instance;
            return instance;
        }

        EthWifiSwitchTest::~EthWifiSwitchTest()
        {
            if (m_worker.joinable())
                m_worker.join();
        }

        void EthWifiSwitchTest::onInternetStatusChange(Exchange::INetworkManager::InternetStatus status, const std::string& iface)
        {
            if (status != Exchange::INetworkManager::INTERNET_NOT_AVAILABLE || iface != nmUtils::ethIface())
                return;

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_running) {
                NMLOG_INFO("switch check already running");
                return;
            }
            if (m_worker.joinable())
                m_worker.join();

            NMLOG_INFO("NO_INTERNET on %s, checking %s", iface.c_str(), nmUtils::wlanIface());
            m_running = true;
            m_worker = std::thread(&EthWifiSwitchTest::switchToWifi, this);
        }

        // Runs on its own thread with a private GMainContext so the event thread is not blocked.
        void EthWifiSwitchTest::switchToWifi()
        {
            const std::string ethIface = nmUtils::ethIface();
            const std::string wlanIface = nmUtils::wlanIface();

            GMainContext* ctx = g_main_context_new();
            g_main_context_push_thread_default(ctx);

            GError* error = nullptr;
            NMClient* client = nm_client_new(nullptr, &error);
            if (client == nullptr) {
                NMLOG_ERROR("nm_client_new failed: %s", error ? error->message : "unknown");
                g_clear_error(&error);
            } else {
                trySwitchToWifi(client, ctx, ethIface, wlanIface);
                destroyClient(client, ctx);
            }

            g_main_context_pop_thread_default(ctx);
            g_main_context_unref(ctx);
            m_running = false;
        }
    }
}
