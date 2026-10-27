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

// The only place that depends on Thunder's deferred JSON-RPC reply conventions. Porting to
// another Thunder release or to CallbackFunction registration should only need changes here.

#include "Module.h"
#include "INetworkManager.h"
#include "NetworkManagerLogger.h"

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
            // Thunder R4 PluginServer.h: a handler returning ~0 gets no response now; JSONRPC::Response(Context, ...) sends it later.
            constexpr uint32_t ReplyLater = static_cast<uint32_t>(~0);
            // Channel id used by in-process IDispatcher::Invoke callers (e.g. UtilsJsonrpcDirectLink); a later reply cannot reach them.
            constexpr uint32_t DirectCallerChannel = static_cast<uint32_t>(~0);

            using Callback = Exchange::INetworkManager::IDiagnosticsCallback;
            // Turns the implementation's (result, response) into the handler's return code and JSON-RPC result string.
            using Shaper = std::function<uint32_t(const uint32_t result, const string& response, string& reply)>;
            // Hands the callback to the implementation; returns the implementation's error code.
            using Starter = std::function<uint32_t(Callback* callback)>;

            class Table : public std::enable_shared_from_this<Table>
            {
            private:
                struct Entry
                {
                    Entry(const Core::JSONRPC::Context& requestContext, const Shaper& shaper, const bool directCaller)
                        : context(requestContext), shape(shaper), direct(directCaller) {}

                    const Core::JSONRPC::Context context;
                    const Shaper shape;
                    const bool direct;
                    bool done = false;
                    uint32_t code = Core::ERROR_NONE;
                    string reply;
                };

                class Reply : public Callback
                {
                public:
                    Reply(const std::shared_ptr<Table>& table, const uint64_t id) : _table(table), _id(id) {}
                    // Released without Completed(): the implementation shut down or its process died.
                    ~Reply() override { _table->Complete(_id, Core::ERROR_UNAVAILABLE, string()); }

                    void Completed(const uint32_t result, const string& response) override { _table->Complete(_id, result, response); }

                    BEGIN_INTERFACE_MAP(Reply)
                    INTERFACE_ENTRY(Callback)
                    END_INTERFACE_MAP

                private:
                    const std::shared_ptr<Table> _table;
                    const uint64_t _id;
                };

            public:
                explicit Table(PluginHost::JSONRPC& owner) : _owner(&owner) {}
                Table(const Table&) = delete;
                Table& operator=(const Table&) = delete;

                // Returns ReplyLater for connected callers; direct callers wait here and get the result synchronously.
                uint32_t Invoke(const Core::JSONRPC::Context& context, const Starter& start, const Shaper& shape, string& reply)
                {
                    // The owner may drop its reference during Close() while a direct caller still waits here.
                    const std::shared_ptr<Table> keepAlive = shared_from_this();
                    const bool direct = (context.ChannelId() == DirectCallerChannel);
                    uint64_t id;
                    {
                        std::lock_guard<std::mutex> guard(_lock);
                        if (_owner == nullptr)
                            return shape(Core::ERROR_UNAVAILABLE, string(), reply);
                        id = ++_lastId;
                        _pending.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(context, shape, direct));
                    }

                    Callback* callback = Core::Service<Reply>::Create<Callback>(keepAlive, id);
                    const uint32_t rc = start(callback);
                    if (rc != Core::ERROR_NONE)
                    {
                        const bool unanswered = Withdraw(id);
                        callback->Release();
                        // Drain()/Close() may already have replied while start() was running.
                        return unanswered ? shape(rc, string(), reply) : ReplyLater;
                    }
                    callback->Release();

                    if (!direct)
                        return ReplyLater;

                    std::unique_lock<std::mutex> guard(_lock);
                    _signal.wait(guard, [this, id] { return _pending.at(id).done; });
                    auto it = _pending.find(id);
                    reply = it->second.reply;
                    const uint32_t code = it->second.code;
                    _pending.erase(it);
                    return code;
                }

                // Answers every pending request as if the implementation had failed with error.
                void Drain(const uint32_t error)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    DrainLocked(error);
                }

                // Drain, then drop every later result: call before the plugin's JSON-RPC dispatcher goes away.
                void Close(const uint32_t error)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    DrainLocked(error);
                    _owner = nullptr;
                }

            private:
                void Complete(const uint64_t id, const uint32_t result, const string& response)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto it = _pending.find(id);
                    if ((it == _pending.end()) || it->second.done)
                        return;
                    if (Answer(it->second, result, response))
                        _pending.erase(it);
                }

                // True if the request was still pending, i.e. no reply has been sent for it.
                bool Withdraw(const uint64_t id)
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    return (_pending.erase(id) > 0);
                }

                void DrainLocked(const uint32_t error)
                {
                    if (!_pending.empty())
                        NMLOG_INFO("Answering %zu pending diagnostics request(s) with error %u", _pending.size(), error);
                    for (auto it = _pending.begin(); it != _pending.end();)
                    {
                        if (!it->second.done && Answer(it->second, error, string()))
                            it = _pending.erase(it);
                        else
                            ++it;
                    }
                }

                // Returns true when the entry has been answered and can be erased; direct entries stay for their waiter.
                bool Answer(Entry& entry, const uint32_t result, const string& response)
                {
                    string reply;
                    const uint32_t code = entry.shape(result, response, reply);
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
        }
    }
}
