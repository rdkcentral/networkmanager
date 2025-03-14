#pragma once

#include <core/core.h>
#include <plugins/plugins.h>

#define EVENT_SUBSCRIBE(__A__, __B__, __C__, __D__)   { plugin->Subscribe(__A__, __B__, __C__); (void)__D__; }
#define EVENT_UNSUBSCRIBE(__A__, __B__, __C__, __D__) { plugin->Unsubscribe(__A__, __B__, __C__); (void)__D__; }

#define DECL_CORE_JSONRPC_CONX                        Core::JSONRPC::Context
#define INIT_CONX(__X__, __Y__)                       connection(__X__, __Y__, "")
#define PLUGINHOST_DISPATCHER                         PluginHost::ILocalDispatcher
#define PLUGINHOST_DISPATCHER_ID                      PluginHost::ILocalDispatcher::ID

