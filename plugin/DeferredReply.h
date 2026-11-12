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

// Deferred JSON-RPC replies for any Thunder plugin. This is the only place that depends on Thunder's
// deferred-reply conventions; porting to another Thunder release or to CallbackFunction registration
// should only need changes here. Depends on nothing but Thunder and the including plugin's Module.h.

#include "Module.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace WPEFramework
{
    namespace Plugin
    {
        namespace DeferredReply
        {
            // Thunder R4 PluginServer.h: a handler returning ~0 gets no response now;
            // JSONRPC::Response(Context, ...) sends it later.
            constexpr uint32_t ReplyLater = static_cast<uint32_t>(~0);
            // Channel id used by in-process IDispatcher::Invoke callers (e.g. UtilsJsonrpcDirectLink);
            // a later reply cannot reach them.
            constexpr uint32_t DirectCallerChannel = static_cast<uint32_t>(~0);

            // Turns a failure code into the handler's return code and its JSON-RPC result (or error text).
            // Method-specific.
            using FailureShaper = std::function<uint32_t(const uint32_t error, string& reply)>;

            class Registry;

            // The right and the duty to answer one deferred request. Move-only; answers at most once.
            // Destroying it unanswered abandons the request, which is then answered as failed with
            // ERROR_UNAVAILABLE.
            class PendingReply
            {
            public:
                PendingReply() = default;
                PendingReply(PendingReply&& other) noexcept : _registry(std::move(other._registry)), _id(other._id) {}
                PendingReply& operator=(PendingReply&& other) noexcept
                {
                    if (this != &other)
                    {
                        Abandon();
                        _registry = std::move(other._registry);
                        _id = other._id;
                    }
                    return *this;
                }
                PendingReply(const PendingReply&) = delete;
                PendingReply& operator=(const PendingReply&) = delete;
                ~PendingReply() { Abandon(); }

                bool IsPending() const { return (_registry != nullptr); }

                // Answers with ERROR_NONE and result as the JSON-RPC result.
                void Respond(const string& result);
                // Answers through the request's FailureShaper.
                void Fail(const uint32_t error);

            private:
                friend class Registry;
                PendingReply(const std::shared_ptr<Registry>& registry, const uint64_t id)
                    : _registry(registry), _id(id) {}
                void Abandon();

                std::shared_ptr<Registry> _registry;
                uint64_t _id = 0;
            };

            // Base for a COM-RPC callback object that completes one deferred request: the derived class
            // implements INTERFACE's methods and answers through _reply.
            template <typename INTERFACE>
            class CallbackReply : public INTERFACE
            {
            public:
                CallbackReply(const CallbackReply&) = delete;
                CallbackReply& operator=(const CallbackReply&) = delete;

                // BEGIN_INTERFACE_MAP cannot be used: AddRef() is not found in a dependent base without this->.
                void* QueryInterface(const uint32_t interfaceNumber) override
                {
                    if (interfaceNumber == INTERFACE::ID)
                    {
                        this->AddRef();
                        return static_cast<void*>(static_cast<INTERFACE*>(this));
                    }
                    if (interfaceNumber == Core::IUnknown::ID)
                    {
                        this->AddRef();
                        return static_cast<void*>(static_cast<Core::IUnknown*>(this));
                    }
                    return nullptr;
                }

            protected:
                explicit CallbackReply(PendingReply&& reply) : _reply(std::move(reply)) {}
                ~CallbackReply() override = default;

                PendingReply _reply;
            };

            // One per plugin: tracks every deferred request of that plugin until it has been answered.
            class Registry : public std::enable_shared_from_this<Registry>
            {
            public:
                // Hands the reply to whatever completes the request; returns the error code of starting it.
                using Starter = std::function<uint32_t(PendingReply&& reply)>;

                explicit Registry(PluginHost::JSONRPC& owner) : _owner(&owner) {}
                Registry(const Registry&) = delete;
                Registry& operator=(const Registry&) = delete;

                // Returns ReplyLater for connected callers; direct callers wait here and get the result
                // synchronously. A start failure is returned synchronously, shaped by onFailure.
                uint32_t Invoke(const Core::JSONRPC::Context& context, const Starter& start,
                                const FailureShaper& onFailure, string& reply)
                {
                    // The owner may drop its reference during Close() while a direct caller still waits here.
                    const std::shared_ptr<Registry> keepAlive = shared_from_this();
                    const bool direct = (context.ChannelId() == DirectCallerChannel);
                    uint64_t id;
                    {
                        std::lock_guard<std::mutex> guard(_lock);
                        if (_owner == nullptr)
                            return onFailure(Core::ERROR_UNAVAILABLE, reply);
                        id = ++_lastId;
                        _pending.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                                         std::forward_as_tuple(context, onFailure, direct));
                    }

                    uint32_t rc;
                    {
                        PendingReply pending(keepAlive, id);
                        rc = start(std::move(pending));
                    }   // A reply the starter did not keep is abandoned here, inside the start window.

                    std::unique_lock<std::mutex> guard(_lock);
                    auto it = _pending.find(id);
                    if (it == _pending.end())
                        return ReplyLater;   // Already answered while start() was running (completion or Drain/Close).

                    if (rc != Core::ERROR_NONE)
                    {
                        _pending.erase(it);
                        guard.unlock();
                        return onFailure(rc, reply);
                    }

                    it->second.starting = false;
                    if (it->second.abandoned && !it->second.done && Answer(it->second, Core::ERROR_UNAVAILABLE))
                        _pending.erase(it);

                    if (!direct)
                        return ReplyLater;

                    _signal.wait(guard, [this, id] { return _pending.at(id).done; });
                    it = _pending.find(id);
                    reply = it->second.reply;
                    const uint32_t code = it->second.code;
                    _pending.erase(it);
                    return code;
                }

                // Answers every pending request as failed with error; returns how many were answered.
                size_t Drain(const uint32_t error)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    return DrainLocked(error);
                }

                // Drain, then drop every later result: call before the plugin's JSON-RPC dispatcher goes away.
                size_t Close(const uint32_t error)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    const size_t answered = DrainLocked(error);
                    _owner = nullptr;
                    return answered;
                }

            private:
                friend class PendingReply;

                struct Entry
                {
                    Entry(const Core::JSONRPC::Context& requestContext, const FailureShaper& failureShaper,
                          const bool directCaller)
                        : context(requestContext), onFailure(failureShaper), direct(directCaller) {}

                    const Core::JSONRPC::Context context;
                    const FailureShaper onFailure;
                    const bool direct;
                    bool starting = true;     // Invoke() is still inside start()
                    bool abandoned = false;   // the PendingReply was destroyed unanswered while starting
                    bool done = false;        // direct callers only: answered, waiting to be collected
                    uint32_t code = Core::ERROR_NONE;
                    string reply;
                };

                void Respond(const uint64_t id, const string& result)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto it = _pending.find(id);
                    if ((it != _pending.end()) && !it->second.done && Send(it->second, Core::ERROR_NONE, result))
                        _pending.erase(it);
                }

                void Fail(const uint64_t id, const uint32_t error)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto it = _pending.find(id);
                    if ((it != _pending.end()) && !it->second.done && Answer(it->second, error))
                        _pending.erase(it);
                }

                void Abandon(const uint64_t id)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto it = _pending.find(id);
                    if ((it == _pending.end()) || it->second.done)
                        return;
                    // While starting, Invoke() decides: a start failure must be answered with its own code.
                    if (it->second.starting)
                        it->second.abandoned = true;
                    else if (Answer(it->second, Core::ERROR_UNAVAILABLE))
                        _pending.erase(it);
                }

                size_t DrainLocked(const uint32_t error)
                {
                    size_t answered = 0;
                    for (auto it = _pending.begin(); it != _pending.end();)
                    {
                        if (it->second.done)
                        {
                            ++it;
                            continue;
                        }
                        ++answered;
                        if (Answer(it->second, error))
                            it = _pending.erase(it);
                        else
                            ++it;
                    }
                    return answered;
                }

                bool Answer(Entry& entry, const uint32_t error)
                {
                    string reply;
                    const uint32_t code = entry.onFailure(error, reply);
                    return Send(entry, code, reply);
                }

                // Returns true when the entry has been answered and can be erased;
                // direct entries stay for their waiter.
                bool Send(Entry& entry, const uint32_t code, const string& reply)
                {
                    if (entry.direct)
                    {
                        entry.done = true;
                        entry.code = code;
                        entry.reply = reply;
                        _signal.notify_all();
                        return false;
                    }

                    if (_owner != nullptr)
                    {
                        if (code == Core::ERROR_NONE)
                        {
                            _owner->Response(entry.context, reply);
                        }
                        else
                        {
                            Core::JSONRPC::Error error;
                            error.SetError(code);
                            if (!reply.empty())
                                error.Text = reply;
                            _owner->Response(entry.context, error);
                        }
                    }
                    return true;
                }

            private:
                std::mutex _lock;
                std::condition_variable _signal;
                PluginHost::JSONRPC* _owner;
                std::map<uint64_t, Entry> _pending;
                uint64_t _lastId = 0;
            };

            inline void PendingReply::Respond(const string& result)
            {
                if (_registry != nullptr)
                {
                    const std::shared_ptr<Registry> registry = std::move(_registry);
                    registry->Respond(_id, result);
                }
            }

            inline void PendingReply::Fail(const uint32_t error)
            {
                if (_registry != nullptr)
                {
                    const std::shared_ptr<Registry> registry = std::move(_registry);
                    registry->Fail(_id, error);
                }
            }

            inline void PendingReply::Abandon()
            {
                if (_registry != nullptr)
                {
                    const std::shared_ptr<Registry> registry = std::move(_registry);
                    registry->Abandon(_id);
                }
            }
        }
    }
}
