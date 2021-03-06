// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_SESSION_H_
#define ATOM_BROWSER_API_ATOM_API_SESSION_H_

#include <string>

#include "atom/browser/api/trackable_object.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "content/public/browser/download_manager.h"
#include "native_mate/handle.h"
#include "net/base/completion_callback.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace mate {
class Arguments;
class Dictionary;
}

namespace net {
class ProxyConfig;
class URLRequestContextGetter;
}

class Profile;

namespace atom {

namespace api {

class Session: public mate::TrackableObject<Session>,
               public content::DownloadManager::Observer {
 public:
  using ResolveProxyCallback = base::Callback<void(std::string)>;

  enum class CacheAction {
    CLEAR,
    STATS,
  };

  // Gets or creates Session from the |browser_context|.
  static mate::Handle<Session> CreateFrom(
      v8::Isolate* isolate, content::BrowserContext* browser_context);

  // Gets the Session of |partition|.
  static mate::Handle<Session> FromPartition(
      v8::Isolate* isolate, const std::string& partition,
      const base::DictionaryValue& options = base::DictionaryValue());

  Profile* browser_context() const { return profile_; }

  // mate::TrackableObject:
  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

  // Methods.
  void ResolveProxy(const GURL& url, ResolveProxyCallback callback);
  template<CacheAction action>
  void DoCacheAction(const net::CompletionCallback& callback);
  void ClearStorageData(mate::Arguments* args);
  void ClearHSTSData(mate::Arguments* args);
  void ClearHistory(mate::Arguments* args);
  void FlushStorageData();
  void SetProxy(const net::ProxyConfig& config, const base::Closure& callback);
  void SetDownloadPath(const base::FilePath& path);
  void EnableNetworkEmulation(const mate::Dictionary& options);
  void DisableNetworkEmulation();
  void SetCertVerifyProc(v8::Local<v8::Value> proc, mate::Arguments* args);
  void SetPermissionRequestHandler(v8::Local<v8::Value> val,
                                   mate::Arguments* args);
  void ClearHostResolverCache(mate::Arguments* args);
  void AllowNTLMCredentialsForDomains(const std::string& domains);
  std::string Partition();
  void SetEnableBrotli(bool enabled);
  v8::Local<v8::Value> ContentSettings(v8::Isolate* isolate);
  v8::Local<v8::Value> Cookies(v8::Isolate* isolate);
  v8::Local<v8::Value> Protocol(v8::Isolate* isolate);
  v8::Local<v8::Value> WebRequest(v8::Isolate* isolate);
  v8::Local<v8::Value> UserPrefs(v8::Isolate* isolate);
  v8::Local<v8::Value> Autofill(v8::Isolate* isolate);
  v8::Local<v8::Value> SpellChecker(v8::Isolate* isolate);
  v8::Local<v8::Value> Extensions(v8::Isolate* isolate);
  bool Equal(Session* session) const;

 protected:
  Session(v8::Isolate* isolate, Profile* browser_context);
  ~Session();

  // content::DownloadManager::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

 private:
  void DefaultDownloadDirectoryChanged();

  // Cached object.
  v8::Global<v8::Value> cookies_;
  v8::Global<v8::Value> protocol_;
  v8::Global<v8::Value> web_request_;
  v8::Global<v8::Value> user_prefs_;
  v8::Global<v8::Value> content_settings_;
  v8::Global<v8::Value> autofill_;
  v8::Global<v8::Value> spell_checker_;
  v8::Global<v8::Value> extensions_;

  // The X-DevTools-Emulate-Network-Conditions-Client-Id.
  std::string devtools_network_emulation_client_id_;

  // The task tracker for the HistoryService callbacks.
  base::CancelableTaskTracker task_tracker_;

  Profile* profile_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_SESSION_H_
