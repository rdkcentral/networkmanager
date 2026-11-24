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

#include "NetworkManagerGnomePropertyFollower.h"
#include "NetworkManagerLogger.h"

namespace WPEFramework
{
    namespace Plugin
    {
        /* Handler id 0 means the connect failed (or was mocked out); never pass it
           to g_signal_handler_disconnect. */
        static void disconnectHandler(GObject* instance, gulong& handlerId)
        {
            if (handlerId != 0 && g_signal_handler_is_connected(instance, handlerId))
                g_signal_handler_disconnect(instance, handlerId);
            handlerId = 0;
        }

        PropertyFollower::PropertyFollower(const char* ownerProperty, Getter getter,
                                           std::vector<std::string> childProperties,
                                           OnChange onChange, gpointer userData)
            : m_ownerProperty(ownerProperty)
            , m_getter(getter)
            , m_childProperties(std::move(childProperties))
            , m_onChange(onChange)
            , m_userData(userData)
        {
        }

        PropertyFollower::~PropertyFollower()
        {
            stop();
        }

        void PropertyFollower::start(GObject* owner)
        {
            /* Take the new reference before stop(): when restarting with the current
               owner, the follower may hold its last reference. */
            GObject* newOwner = (owner != nullptr) ? G_OBJECT(g_object_ref(owner)) : nullptr;
            stop();
            if (newOwner == nullptr)
                return;

            m_owner = newOwner;
            const std::string signal = "notify::" + m_ownerProperty;
            m_ownerHandler = g_signal_connect(m_owner, signal.c_str(), G_CALLBACK(ownerNotifyCb), this);
            bindChild(m_getter(m_owner));
        }

        void PropertyFollower::stop()
        {
            unbindChild();
            if (m_owner != nullptr)
            {
                disconnectHandler(m_owner, m_ownerHandler);
                g_object_unref(m_owner);
                m_owner = nullptr;
            }
        }

        void PropertyFollower::bindChild(GObject* child)
        {
            if (child == m_child)
                return;

            unbindChild();
            if (child == nullptr)
                return;

            m_child = G_OBJECT(g_object_ref(child));
            for (const auto& property : m_childProperties)
            {
                const std::string signal = "notify::" + property;
                gulong id = g_signal_connect(m_child, signal.c_str(), G_CALLBACK(childNotifyCb), this);
                if (id != 0)
                    m_childHandlers.push_back(id);
            }
            NMLOG_DEBUG("following new '%s' object", m_ownerProperty.c_str());
        }

        void PropertyFollower::unbindChild()
        {
            if (m_child == nullptr)
                return;

            for (auto& id : m_childHandlers)
                disconnectHandler(m_child, id);
            m_childHandlers.clear();
            g_object_unref(m_child);
            m_child = nullptr;
        }

        void PropertyFollower::ownerNotifyCb(GObject* owner, GParamSpec* pspec, gpointer self)
        {
            PropertyFollower* follower = static_cast<PropertyFollower*>(self);
            if (follower->m_owner == nullptr)
                return;
            follower->bindChild(follower->m_getter(follower->m_owner));
            if (follower->m_onChange)
                follower->m_onChange(follower->m_owner, follower->m_userData);
        }

        void PropertyFollower::childNotifyCb(GObject* child, GParamSpec* pspec, gpointer self)
        {
            PropertyFollower* follower = static_cast<PropertyFollower*>(self);
            if (follower->m_owner == nullptr || child != follower->m_child)
                return;
            if (follower->m_onChange)
                follower->m_onChange(follower->m_owner, follower->m_userData);
        }
    }   // Plugin
}   // WPEFramework
