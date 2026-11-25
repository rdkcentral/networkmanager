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

#pragma once
#include <glib-object.h>
#include <string>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {
        /* Keeps "notify::<property>" handlers attached only to the GObject currently
           held in a property of an owner object (e.g. a WiFi device's active access
           point). When the owner's property changes, the handlers move from the old
           object to the new one. onChange runs on every owner-property change and on
           every watched child-property change.

           Not thread-safe: create, start, stop and receive signals on one thread
           (the event monitor's main context). */
        class PropertyFollower
        {
        public:
            using Getter   = GObject* (*)(GObject* owner);
            using OnChange = void (*)(GObject* owner, gpointer userData);

            PropertyFollower(const char* ownerProperty, Getter getter,
                             std::vector<std::string> childProperties,
                             OnChange onChange, gpointer userData);
            ~PropertyFollower();

            PropertyFollower(const PropertyFollower&) = delete;
            PropertyFollower& operator=(const PropertyFollower&) = delete;
            PropertyFollower(PropertyFollower&&) = delete;
            PropertyFollower& operator=(PropertyFollower&&) = delete;

            /* Idempotent: stops any previous binding first. Does not call onChange. */
            void start(GObject* owner);
            void stop();
            GObject* current() const { return m_child; }

        private:
            static void ownerNotifyCb(GObject* owner, GParamSpec* pspec, gpointer self);
            static void childNotifyCb(GObject* child, GParamSpec* pspec, gpointer self);
            void bindChild(GObject* child);
            void unbindChild();

            const std::string m_ownerProperty;
            const Getter m_getter;
            const std::vector<std::string> m_childProperties;
            const OnChange m_onChange;
            const gpointer m_userData;

            GObject* m_owner = nullptr;
            gulong m_ownerHandler = 0;
            GObject* m_child = nullptr;
            std::vector<gulong> m_childHandlers;
        };
    }   // Plugin
}   // WPEFramework
