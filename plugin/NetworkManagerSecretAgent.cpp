
/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <glib.h>
#include <gio/gio.h>

#include <NetworkManager.h>
#include <libnm/NetworkManager.h>

#include "NetworkManagerLogger.h"
#include "NetworkManagerSecretAgent.h"

namespace WPEFramework
{
    namespace Plugin
    {
        std::condition_variable m_SecretsAgentCv;
        std::mutex m_SecretsAgentMutex;
        bool cvCondition = false;  // avoid spurious wakeup
        static const gchar interfaceXml[] =
            "<node>"
            "  <interface name='org.freedesktop.NetworkManager.SecretAgent'>"
            "    <method name='GetSecrets'>"
            "      <arg type='a{sa{sv}}' name='connection' direction='in'/>"
            "      <arg type='o' name='connection_path' direction='in'/>"
            "      <arg type='s' name='setting_name' direction='in'/>"
            "      <arg type='as' name='hints' direction='in'/>"
            "      <arg type='u' name='flags' direction='in'/>"
            "      <arg type='a{sa{sv}}' name='secrets' direction='out'/>"
            "    </method>"
            "    <method name='CancelGetSecrets'>"
            "      <arg type='o' name='connection_path' direction='in'/>"
            "      <arg type='s' name='setting_name' direction='in'/>"
            "    </method>"
            "    <method name='SaveSecrets'>"
            "      <arg type='o' name='connection_path' direction='in'/>"
            "      <arg type='s' name='setting_name' direction='in'/>"
            "    </method>"
            "    <method name='DeleteSecrets'>"
            "      <arg type='o' name='connection_path' direction='in'/>"
            "      <arg type='s' name='setting_name' direction='in'/>"
            "    </method>"
            "  </interface>"
            "</node>";

        static void handleSecretsAgentMethods( GDBusConnection *connection, const gchar *sender,
            const gchar *object_path, const gchar *interface_name, const gchar *method_name,
            GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data) {

            NMLOG_DEBUG("handleSecretsAgentMethods called for sender: %s", sender);
            char *string = g_variant_print(parameters, TRUE);
            NMLOG_DEBUG("GVariant: %s", string);
            g_free(string);

            if (g_strcmp0(method_name, "GetSecrets") == 0)
            {
                GVariant *settingConn = NULL;
                GVariant *connectionPath = NULL;
                GVariant *settingName = NULL;
                GVariant *hints = NULL;
                GVariant *flagVar = NULL;
                guint flags=0;

                g_variant_get(parameters, "(@a{sa{sv}}@o@s@as@u)",  &settingConn,  &connectionPath, &settingName, &hints, &flagVar);

                if (g_variant_is_of_type (connectionPath, G_VARIANT_TYPE_OBJECT_PATH)) {
                    const char* connPath = g_variant_get_string(connectionPath, NULL);
                    NMLOG_DEBUG("connection path %s", connPath);
                }

                if (g_variant_is_of_type (flagVar, G_VARIANT_TYPE_UINT32))
                {
                    std::string flagStr;
                    flags = g_variant_get_uint32(flagVar);
                    if(flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_WPS_PBC_ACTIVE)
                        flagStr += ", wps_pbc_active";
                    if(flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_ALLOW_INTERACTION)
                        flagStr += ", allow_interaction";
                    if(flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_USER_REQUESTED) {
                        flagStr += ", user_requested";
                        /* wait for 10 sec mean time in the background networkmanager will do wps operation */
                        SecretAgent::wait(10); // 10 sec
                    }
                    NMLOG_INFO("SecretAgent GetSecrets Flags = %u %s", flags, flagStr.c_str());
                }

                if(settingConn)
                    g_variant_unref(settingConn);
                if(connectionPath)
                    g_variant_unref(connectionPath);
                if(hints)
                    g_variant_unref(hints);
                if(flagVar)
                    g_variant_unref(flagVar);

                g_dbus_method_invocation_return_value(invocation, g_variant_new("(a{sa{sv}})", NULL));
            }
            else if (g_strcmp0(method_name, "CancelGetSecrets") == 0)
            {
                NMLOG_DEBUG("CancelGetSecrets method called");
                SecretAgent::stopWait();
                g_dbus_method_invocation_return_value(invocation, NULL);
            }
            else if (g_strcmp0(method_name, "SaveSecrets") == 0)
            {
                NMLOG_DEBUG("SaveSecrets method called");
                g_dbus_method_invocation_return_value(invocation, NULL);
            }
            else if (g_strcmp0(method_name, "DeleteSecrets") == 0)
            {
                NMLOG_DEBUG("DeleteSecrets method called");
                g_dbus_method_invocation_return_value(invocation, NULL);
            }

            NMLOG_DEBUG("method: %s", method_name);
        }

        static const GDBusInterfaceVTable interfaceVtable = {
            .method_call = handleSecretsAgentMethods,
            .get_property = NULL,
            .set_property = NULL,
        };

        SecretAgent::SecretAgent()
        {
            GError *error = NULL;
            isSecretAgentLoopRunning = false;
            NMLOG_INFO("SecretAgent Constructor");
            GDBusconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
            if (!GDBusconn) {
                NMLOG_ERROR("Failed to connect to system bus: %s", error->message);
                g_error_free(error);
                return;
            }

            isSecurityAgentRegistered = false;
            startSecurityAgent();
        }

        SecretAgent::~SecretAgent()
        {
            NMLOG_INFO("SecretAgent Destructor");
            stopSecurityAgent();
            if(GDBusconn)
                g_object_unref(GDBusconn);
        }

        void SecretAgent::wait(int timeoutInSec)
        {
            NMLOG_INFO("wait started %d Sec", timeoutInSec);
            std::unique_lock<std::mutex> lock(m_SecretsAgentMutex);
            if (m_SecretsAgentCv.wait_for(lock, std::chrono::seconds(timeoutInSec), [] { return cvCondition; })) {
                NMLOG_INFO("SecretAgent received a cancel request. skipping %d sec wait", timeoutInSec);
                // Reset the flag to avoid spurious wakeup on a condition variable
                cvCondition = false;
            } else {
                NMLOG_INFO("SecretAgent wait time %d sec complete", timeoutInSec);
            }
        }

        void SecretAgent::stopWait()
        {
            NMLOG_INFO("stopWait Entry");
            {
                std::unique_lock<std::mutex> lock(m_SecretsAgentMutex);
                cvCondition = true;
            }
            m_SecretsAgentCv.notify_one();
            NMLOG_INFO("stopWait Exit");
        }

        void SecretAgent::SecretAgentLoop(SecretAgent* SecretAgentPtr)
        {
            GError *error = NULL;
            GMainContext *secretAgentContext = NULL;
            SecretAgent* agent = static_cast<SecretAgent*>(SecretAgentPtr);
            secretAgentContext = g_main_context_new();
            if(secretAgentContext == NULL)
            {
                NMLOG_ERROR("Failed to create Security Agent gmain loop context");
                return;
            }

            g_main_context_push_thread_default(secretAgentContext);
            agent->agentGmainLoop = g_main_loop_new(secretAgentContext, FALSE);
            if(agent->agentGmainLoop == NULL)
            {
                NMLOG_ERROR("Failed to create Security Agent gmain loop");
                g_main_context_pop_thread_default(secretAgentContext);
                g_main_context_unref(secretAgentContext);
                return;
            }

            GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(interfaceXml, &error);
            if (!node_info) {
                NMLOG_ERROR("Failed to parse XML: %s", error->message);
                g_error_free(error);
                return;
            }

            guint agentRegID = g_dbus_connection_register_object(
                            agent->GDBusconn,
                            "/org/freedesktop/NetworkManager/SecretAgent",
                            g_dbus_node_info_lookup_interface(node_info, "org.freedesktop.NetworkManager.SecretAgent"),  // Interface,
                            &interfaceVtable,
                            NULL,
                            NULL,
                            &error);

            if (!agentRegID) {
                NMLOG_ERROR("Failed to register object: %s", error->message);
                g_error_free(error);
                g_dbus_node_info_unref(node_info);
                return ;
            }

            NMLOG_INFO("Object registered with ID: %u", agentRegID);
            NMLOG_DEBUG("Security Agent gmain loop running...");
            g_main_loop_run(agent->agentGmainLoop);

            g_main_loop_unref(agent->agentGmainLoop);
            g_dbus_connection_unregister_object(agent->GDBusconn, agentRegID);
            g_dbus_node_info_unref(node_info);
            g_main_context_pop_thread_default(secretAgentContext);
            g_main_context_unref(secretAgentContext);
            agent->isSecretAgentLoopRunning = false;
            NMLOG_WARNING("Security Agent loop ended !");
        }

        void SecretAgent::startSecurityAgent()
        {
            NMLOG_DEBUG("staring Security Agent...");
            if(isSecretAgentLoopRunning)
            {
                NMLOG_INFO("Security Agent already running.");
                return;
            }

            isSecretAgentLoopRunning = true;
            agentThrID = std::thread(&SecretAgentLoop, this);
        }

        void SecretAgent::stopSecurityAgent()
        {
            stopWait();
            if (agentGmainLoop) {
                g_main_loop_quit(agentGmainLoop);
            }

            if (agentThrID.joinable()) {
                agentThrID.join();
            }

            NMLOG_INFO("Security Agent stopped.");
        }

        bool SecretAgent::RegisterAgent()
        {
            if(isSecurityAgentRegistered)
            {
                NMLOG_INFO("'rdk.nm.agent' alreday registered to NetworkManager");
                return true;
            }

            GError *error = NULL;
            GDBusProxy *nmAgentProxy = g_dbus_proxy_new_for_bus_sync (
                G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.NetworkManager",
                "/org/freedesktop/NetworkManager/AgentManager",
                "org.freedesktop.NetworkManager.AgentManager",
                NULL,
                &error
            );

            if (error != NULL && nmAgentProxy == NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return false;
            }

            g_dbus_proxy_call_sync( nmAgentProxy, "Register",
                g_variant_new("(s)", "rdk.nm.agent"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

            if (error != NULL) {
                NMLOG_ERROR("Failed to register agent: %s", error->message);
                g_error_free(error);
                return false;
            }

            NMLOG_INFO("'rdk.nm.agent' registered to NetworkManager");
            isSecurityAgentRegistered = true;
            if(nmAgentProxy)
                g_object_unref(nmAgentProxy);
            return true;
        }

        bool SecretAgent::UnregisterAgent()
        {
            if(!isSecurityAgentRegistered)
            {
                NMLOG_INFO("'rdk.nm.agent' not registered to NetworkManager");
                return true;
            }

            GError *error = NULL;
            GDBusProxy *nmAgentProxy = g_dbus_proxy_new_for_bus_sync (
                G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.NetworkManager",
                "/org/freedesktop/NetworkManager/AgentManager",
                "org.freedesktop.NetworkManager.AgentManager",
                NULL,
                &error
            );

            if (error != NULL && nmAgentProxy == NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return false;
            }

            g_dbus_proxy_call_sync(nmAgentProxy, "Unregister", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

            if (error != NULL) {
                NMLOG_FATAL("Failed to call Unregister agent: %s", error->message);
                g_error_free(error);
                return false;
            }

            NMLOG_INFO("'rdk.nm.agent' successfully Unregister");
            isSecurityAgentRegistered = false;
            if(nmAgentProxy)
                g_object_unref(nmAgentProxy);
            return true;
        }

    } // WPEFramework
} // Plugin
