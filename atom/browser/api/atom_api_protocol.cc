// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_protocol.h"

#include "atom/browser/atom_browser_client.h"
#include "atom/browser/atom_browser_main_parts.h"
#include "atom/browser/browser.h"
#include "atom/browser/net/url_request_buffer_job.h"
#include "atom/browser/net/url_request_fetch_job.h"
#include "atom/browser/net/url_request_string_job.h"
#include "atom/common/native_mate_converters/callback.h"
#include "atom/common/native_mate_converters/v8_value_converter.h"
#include "atom/common/native_mate_converters/value_converter.h"
#include "atom/common/node_includes.h"
#include "atom/common/options_switches.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "content/public/browser/child_process_security_policy.h"
#include "native_mate/dictionary.h"
#include "url/url_util.h"

namespace mate {

template<>
struct Converter<const base::ListValue*> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      const base::ListValue* val) {
    std::unique_ptr<atom::V8ValueConverter>
        converter(new atom::V8ValueConverter);
    return converter->ToV8Value(val, isolate->GetCurrentContext());
  }
};

}  // namespace mate

using content::BrowserThread;

namespace atom {

namespace api {

// TODO(bridiver)
// https://github.com/electron/electron/commit/1beba5bdc086671bed9205faa694817f5a07c6ad
// causes a hang on shutdown
namespace {

// List of registered custom standard schemes.
std::vector<std::string> g_standard_schemes;

}  // namespace

std::vector<std::string> GetStandardSchemes() {
  return g_standard_schemes;
}

void RegisterStandardSchemes(const std::vector<std::string>& schemes) {
  g_standard_schemes = schemes;

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  for (const std::string& scheme : schemes) {
    url::AddStandardScheme(scheme.c_str(), url::SCHEME_WITH_HOST);
    policy->RegisterWebSafeScheme(scheme);
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      atom::switches::kStandardSchemes, base::JoinString(schemes, ","));
}

Protocol::Protocol(v8::Isolate* isolate, Profile* profile)
    : profile_(profile),
      request_context_getter_(static_cast<brightray::URLRequestContextGetter*>(
          profile->GetRequestContext())),
      weak_factory_(this) {
  Init(isolate);
}

Protocol::~Protocol() {
}

void Protocol::RegisterServiceWorkerSchemes(
    const std::vector<std::string>& schemes) {
  atom::AtomBrowserClient::SetCustomServiceWorkerSchemes(schemes);
}

// Register the protocol with certain request job.
template<typename RequestJob>
void Protocol::RegisterProtocol(const std::string& scheme,
                      const Handler& handler,
                      mate::Arguments* args) {
  CompletionCallback callback;
  args->GetNext(&callback);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&Protocol::RegisterProtocolInIO<RequestJob>,
          request_context_getter_,
          isolate(), scheme, handler),
      base::Bind(&Protocol::OnIOCompleted,
                 GetWeakPtr(), callback));
}

// static
template<typename RequestJob>
Protocol::ProtocolError Protocol::RegisterProtocolInIO(
    scoped_refptr<brightray::URLRequestContextGetter> request_context_getter,
    v8::Isolate* isolate,
    const std::string& scheme,
    const Handler& handler) {
  auto job_factory = static_cast<net::URLRequestJobFactoryImpl*>(
      request_context_getter->job_factory());
  if (job_factory->IsHandledProtocol(scheme))
    return PROTOCOL_REGISTERED;
  std::unique_ptr<CustomProtocolHandler<RequestJob>> protocol_handler(
      new CustomProtocolHandler<RequestJob>(
          isolate, request_context_getter.get(), handler));
  if (job_factory->SetProtocolHandler(scheme, std::move(protocol_handler)))
    return PROTOCOL_OK;
  else
    return PROTOCOL_FAIL;
}

void Protocol::UnregisterProtocol(
    const std::string& scheme, mate::Arguments* args) {
  CompletionCallback callback;
  args->GetNext(&callback);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&Protocol::UnregisterProtocolInIO,
          request_context_getter_, scheme),
      base::Bind(&Protocol::OnIOCompleted,
                 GetWeakPtr(), callback));
}

// static
Protocol::ProtocolError Protocol::UnregisterProtocolInIO(
    scoped_refptr<brightray::URLRequestContextGetter> request_context_getter,
    const std::string& scheme) {
  auto job_factory = static_cast<net::URLRequestJobFactoryImpl*>(
      request_context_getter->job_factory());
  if (!job_factory->IsHandledProtocol(scheme))
    return PROTOCOL_NOT_REGISTERED;
  job_factory->SetProtocolHandler(scheme, nullptr);
  return PROTOCOL_OK;
}

void Protocol::IsProtocolHandled(const std::string& scheme,
                                 const BooleanCallback& callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&Protocol::IsProtocolHandledInIO,
          request_context_getter_, scheme),
      callback);
}

// static
bool Protocol::IsProtocolHandledInIO(
    scoped_refptr<brightray::URLRequestContextGetter> request_context_getter,
    const std::string& scheme) {
  return request_context_getter->job_factory()->IsHandledProtocol(scheme);
}

const base::ListValue*
Protocol::GetNavigatorHandlers() {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          profile_);
  std::vector<std::string> protocols;
  registry->GetRegisteredProtocols(&protocols);

  auto* result = new base::ListValue();
  std::for_each(protocols.begin(),
                protocols.end(),
                [&registry, &result] (const std::string &protocol) {
                  ProtocolHandler handler = registry->GetHandlerFor(protocol);
                  auto* dict = new base::DictionaryValue();
                  dict->SetString("protocol", handler.protocol());
                  dict->SetString("location", handler.url().spec());
                  result->Append(std::unique_ptr<base::DictionaryValue>(dict));
                });


  return result;
}

void Protocol::UnregisterNavigatorHandler(const std::string& scheme,
                                          const std::string& spec) {
  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(scheme, GURL(spec));
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          profile_);
  registry->RemoveHandler(handler);
}

void Protocol::RegisterNavigatorHandler(const std::string& scheme,
                                        const std::string& spec) {
  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(scheme, GURL(spec));
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          profile_);
  registry->OnAcceptRegisterProtocolHandler(handler);
}

bool Protocol::IsNavigatorProtocolHandled(const std::string& scheme) {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          profile_);
  return registry->IsHandledProtocol(scheme);
}

void Protocol::OnIOCompleted(
    const CompletionCallback& callback, ProtocolError error) {
  // The completion callback is optional.
  if (callback.is_null())
    return;

  v8::Locker locker(isolate());
  v8::HandleScope handle_scope(isolate());

  if (error == PROTOCOL_OK) {
    callback.Run(v8::Null(isolate()));
  } else {
    std::string str = ErrorCodeToString(error);
    callback.Run(v8::Exception::Error(mate::StringToV8(isolate(), str)));
  }
}

std::string Protocol::ErrorCodeToString(ProtocolError error) {
  switch (error) {
    case PROTOCOL_FAIL: return "Failed to manipulate protocol factory";
    case PROTOCOL_REGISTERED: return "The scheme has been registered";
    case PROTOCOL_NOT_REGISTERED: return "The scheme has not been registered";
    case PROTOCOL_INTERCEPTED: return "The scheme has been intercepted";
    case PROTOCOL_NOT_INTERCEPTED: return "The scheme has not been intercepted";
    default: return "Unexpected error";
  }
}

// static
mate::Handle<Protocol> Protocol::Create(
    v8::Isolate* isolate, content::BrowserContext* browser_context) {
  return mate::CreateHandle(isolate,
      new Protocol(isolate, Profile::FromBrowserContext(browser_context)));
}

// static
void Protocol::BuildPrototype(
    v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "Protocol"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("registerServiceWorkerSchemes",
                 &Protocol::RegisterServiceWorkerSchemes)
      .SetMethod("registerStringProtocol",
                 &Protocol::RegisterProtocol<URLRequestStringJob>)
      .SetMethod("registerBufferProtocol",
                 &Protocol::RegisterProtocol<URLRequestBufferJob>)
      .SetMethod("registerHttpProtocol",
                 &Protocol::RegisterProtocol<URLRequestFetchJob>)
      .SetMethod("unregisterProtocol", &Protocol::UnregisterProtocol)
      .SetMethod("isProtocolHandled", &Protocol::IsProtocolHandled)
      .SetMethod("isNavigatorProtocolHandled",
                 &Protocol::IsNavigatorProtocolHandled)
      .SetMethod("getNavigatorHandlers",
                 &Protocol::GetNavigatorHandlers)
      .SetMethod("registerNavigatorHandler",
                 &Protocol::RegisterNavigatorHandler)
      .SetMethod("unregisterNavigatorHandler",
                 &Protocol::UnregisterNavigatorHandler);
}

}  // namespace api

}  // namespace atom

namespace {

void RegisterStandardSchemes(
    const std::vector<std::string>& schemes, mate::Arguments* args) {
  if (atom::Browser::Get()->is_ready()) {
    args->ThrowError("protocol.registerStandardSchemes should be called before "
                     "app is ready");
    return;
  }

  atom::api::RegisterStandardSchemes(schemes);
}

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context, void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  mate::Dictionary dict(isolate, exports);
  dict.SetMethod("registerStandardSchemes", &RegisterStandardSchemes);
  dict.SetMethod("getStandardSchemes", &atom::api::GetStandardSchemes);
}

}  // namespace

NODE_MODULE_CONTEXT_AWARE_BUILTIN(atom_browser_protocol, Initialize)
