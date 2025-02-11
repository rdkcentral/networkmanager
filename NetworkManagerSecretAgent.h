/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 */

#pragma once
#include <gio/gio.h>
#include <iostream>
#include <string>
#include <list>

#include "NetworkManagerLogger.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class SecretAgent
        {
            public:
                SecretAgent();
                ~SecretAgent();
                void startSecurityAgent();
                void stopSecurityAgent();
                bool RegisterAgent();
                bool UnregisterAgent();
                static void SecretAgentLoop(SecretAgent* agent);
                static void wait(const std::chrono::seconds timeout);
                static void stopWait();
            private:
                std::atomic<bool> isSecretAgentLoopRunning;
                GMainLoop *agentGmainLoop = nullptr;
                GDBusConnection *GDBusconn;
                std::thread agentThrID;
                std::atomic<bool> isSecurityAgentRegistered;
        };
    } // Plugin
} // WPEFramework
