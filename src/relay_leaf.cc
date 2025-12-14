#include <napi.h>
#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>

// ============================================================
// Relay Leaf C API signatures (from your header)
// ============================================================

typedef uintptr_t RelayLeafHandle;

typedef struct RelayLeafStats {
  int64_t uptime_seconds;
  int64_t total_streams;
  int64_t bytes_sent;
  int64_t bytes_received;
  int64_t reconnect_count;
  char* last_error;             // must free via relay_leaf_free_stats()
  char* exit_points_json;       // must free via relay_leaf_free_stats()
  char* node_addresses_json;    // must free via relay_leaf_free_stats()
  int32_t active_streams;
  int32_t connected_nodes;
  bool connected;
} RelayLeafStats;

// Functions
typedef int32_t (*fn_relay_leaf_create)(bool verbose, RelayLeafHandle* out_handle);
typedef int32_t (*fn_relay_leaf_set_discovery_url)(RelayLeafHandle handle, const char* url);
typedef int32_t (*fn_relay_leaf_set_partner_id)(RelayLeafHandle handle, const char* partner_id);
typedef int32_t (*fn_relay_leaf_add_proxy)(RelayLeafHandle handle, const char* proxy_url);
typedef int32_t (*fn_relay_leaf_start)(RelayLeafHandle handle);
typedef int32_t (*fn_relay_leaf_stop)(RelayLeafHandle handle);
typedef int32_t (*fn_relay_leaf_destroy)(RelayLeafHandle handle);
typedef int32_t (*fn_relay_leaf_get_stats)(RelayLeafHandle handle, RelayLeafStats* out_stats);
typedef void    (*fn_relay_leaf_free_stats)(RelayLeafStats* stats);
typedef void    (*fn_relay_leaf_free_string)(char* str);
typedef char*   (*fn_relay_leaf_version)(void);
typedef char*   (*fn_relay_leaf_error_message)(int32_t code);
typedef char*   (*fn_relay_leaf_get_device_id)(RelayLeafHandle handle);

// ============================================================
// DLL loader (loads relay_leaf_win64.dll from same folder as addon)
// ============================================================

static HMODULE g_dll = NULL;

static fn_relay_leaf_create           g_create = NULL;
static fn_relay_leaf_set_discovery_url g_set_discovery_url = NULL;
static fn_relay_leaf_set_partner_id    g_set_partner_id = NULL;
static fn_relay_leaf_add_proxy         g_add_proxy = NULL;
static fn_relay_leaf_start             g_start = NULL;
static fn_relay_leaf_stop              g_stop = NULL;
static fn_relay_leaf_destroy           g_destroy = NULL;
static fn_relay_leaf_get_stats         g_get_stats = NULL;
static fn_relay_leaf_free_stats        g_free_stats = NULL;
static fn_relay_leaf_free_string       g_free_string = NULL;
static fn_relay_leaf_version           g_version = NULL;
static fn_relay_leaf_error_message     g_error_message = NULL;
static fn_relay_leaf_get_device_id     g_get_device_id = NULL;

static std::string GetAddonDir() {
  // Find the path of this module (.node) at runtime.
  HMODULE hm = NULL;
  if (!GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&GetAddonDir,
        &hm)) {
    return ".";
  }

  char path[MAX_PATH]{0};
  DWORD len = GetModuleFileNameA(hm, path, MAX_PATH);
  if (len == 0) return ".";

  std::string full(path);
  size_t pos = full.find_last_of("\\/");
  if (pos == std::string::npos) return ".";
  return full.substr(0, pos);
}

static FARPROC MustGetProc(const char* name) {
  FARPROC p = GetProcAddress(g_dll, name);
  if (!p) {
    std::string msg = "Missing export in relay_leaf_win64.dll: ";
    msg += name;
    throw std::runtime_error(msg);
  }
  return p;
}

static void EnsureLoaded() {
  if (g_dll) return;

  const std::string dir = GetAddonDir();
  const std::string dllPath = dir + "\\relay_leaf_win64.dll";

  g_dll = LoadLibraryA(dllPath.c_str());
  if (!g_dll) {
    DWORD e = GetLastError();
    std::string msg = "Failed to LoadLibrary relay_leaf_win64.dll from: " + dllPath +
                      " (GetLastError=" + std::to_string((int)e) + ")";
    throw std::runtime_error(msg);
  }

  g_create            = (fn_relay_leaf_create)MustGetProc("relay_leaf_create");
  g_set_discovery_url = (fn_relay_leaf_set_discovery_url)MustGetProc("relay_leaf_set_discovery_url");
  g_set_partner_id    = (fn_relay_leaf_set_partner_id)MustGetProc("relay_leaf_set_partner_id");
  g_add_proxy         = (fn_relay_leaf_add_proxy)MustGetProc("relay_leaf_add_proxy");
  g_start             = (fn_relay_leaf_start)MustGetProc("relay_leaf_start");
  g_stop              = (fn_relay_leaf_stop)MustGetProc("relay_leaf_stop");
  g_destroy           = (fn_relay_leaf_destroy)MustGetProc("relay_leaf_destroy");
  g_get_stats         = (fn_relay_leaf_get_stats)MustGetProc("relay_leaf_get_stats");
  g_free_stats        = (fn_relay_leaf_free_stats)MustGetProc("relay_leaf_free_stats");
  g_free_string       = (fn_relay_leaf_free_string)MustGetProc("relay_leaf_free_string");
  g_version           = (fn_relay_leaf_version)MustGetProc("relay_leaf_version");
  g_error_message     = (fn_relay_leaf_error_message)MustGetProc("relay_leaf_error_message");
  g_get_device_id     = (fn_relay_leaf_get_device_id)MustGetProc("relay_leaf_get_device_id");
}

static Napi::Error JsError(Napi::Env env, const std::string& s) {
  return Napi::Error::New(env, s);
}

static std::string OwnedCStringToStdStringAndFree(char* p) {
  if (!p) return "";
  std::string s(p);
  g_free_string(p);
  return s;
}

static std::string ErrorMessage(int32_t code) {
  EnsureLoaded();
  char* p = g_error_message(code);
  return OwnedCStringToStdStringAndFree(p);
}

// ============================================================
// RelayClient class wrapper
// ============================================================

class RelayClient : public Napi::ObjectWrap<RelayClient> {
public:
  static Napi::Function GetClass(Napi::Env env) {
    return DefineClass(env, "RelayClient", {
      InstanceMethod("setDiscoveryUrl", &RelayClient::SetDiscoveryUrl),
      InstanceMethod("setPartnerId", &RelayClient::SetPartnerId),
      InstanceMethod("addProxy", &RelayClient::AddProxy),
      InstanceMethod("start", &RelayClient::Start),
      InstanceMethod("stop", &RelayClient::Stop),
      InstanceMethod("destroy", &RelayClient::Destroy),
      InstanceMethod("deviceId", &RelayClient::DeviceId),
      InstanceMethod("stats", &RelayClient::Stats),
      InstanceAccessor("handle", &RelayClient::GetHandle, nullptr),
    });
  }

  RelayClient(const Napi::CallbackInfo& info) : Napi::ObjectWrap<RelayClient>(info) {
    Napi::Env env = info.Env();
    EnsureLoaded();

    bool verbose = false;
    if (info.Length() >= 1 && info[0].IsObject()) {
      Napi::Object opts = info[0].As<Napi::Object>();
      if (opts.Has("verbose") && opts.Get("verbose").IsBoolean()) {
        verbose = opts.Get("verbose").As<Napi::Boolean>().Value();
      }
    }

    RelayLeafHandle h = 0;
    int32_t rc = g_create(verbose, &h);
    if (rc != 0) {
      std::string msg = "relay_leaf_create failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")";
      throw JsError(env, msg);
    }

    handle_ = h;
    destroyed_ = false;
  }

  ~RelayClient() {
    // Be conservative: do not auto-destroy if JS forgot.
    // If you want RAII behavior, uncomment this block.
    /*
    if (!destroyed_ && handle_ != 0) {
      g_destroy(handle_);
      handle_ = 0;
      destroyed_ = true;
    }
    */
  }

private:
  RelayLeafHandle handle_ = 0;
  bool destroyed_ = false;

  void EnsureValid(Napi::Env env) {
    if (destroyed_ || handle_ == 0) {
      throw JsError(env, "RelayClient is destroyed");
    }
  }

  Napi::Value GetHandle(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    // BigInt for 64-bit safety
    return Napi::BigInt::New(env, (uint64_t)handle_);
  }

  Napi::Value SetDiscoveryUrl(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    if (info.Length() < 1 || !info[0].IsString()) {
      throw JsError(env, "setDiscoveryUrl(url) requires a string");
    }
    std::string url = info[0].As<Napi::String>().Utf8Value();
    int32_t rc = g_set_discovery_url(handle_, url.c_str());
    if (rc != 0) {
      throw JsError(env, "relay_leaf_set_discovery_url failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }
    return env.Undefined();
  }

  Napi::Value SetPartnerId(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    if (info.Length() < 1 || !info[0].IsString()) {
      throw JsError(env, "setPartnerId(partnerId) requires a string");
    }
    std::string pid = info[0].As<Napi::String>().Utf8Value();
    int32_t rc = g_set_partner_id(handle_, pid.c_str());
    if (rc != 0) {
      throw JsError(env, "relay_leaf_set_partner_id failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }
    return env.Undefined();
  }

  Napi::Value AddProxy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    if (info.Length() < 1 || !info[0].IsString()) {
      throw JsError(env, "addProxy(proxyUrl) requires a string");
    }
    std::string p = info[0].As<Napi::String>().Utf8Value();
    int32_t rc = g_add_proxy(handle_, p.c_str());
    if (rc != 0) {
      throw JsError(env, "relay_leaf_add_proxy failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }
    return env.Undefined();
  }

  Napi::Value Start(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    int32_t rc = g_start(handle_);
    if (rc != 0) {
      throw JsError(env, "relay_leaf_start failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }
    return env.Undefined();
  }

  Napi::Value Stop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    int32_t rc = g_stop(handle_);
    if (rc != 0) {
      throw JsError(env, "relay_leaf_stop failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }
    return env.Undefined();
  }

  Napi::Value Destroy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!destroyed_ && handle_ != 0) {
      int32_t rc = g_destroy(handle_);
      if (rc != 0) {
        throw JsError(env, "relay_leaf_destroy failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
      }
      handle_ = 0;
      destroyed_ = true;
    }
    return env.Undefined();
  }

  Napi::Value DeviceId(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);
    char* p = g_get_device_id(handle_);
    std::string s = OwnedCStringToStdStringAndFree(p);
    return Napi::String::New(env, s);
  }

  Napi::Value Stats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EnsureValid(env);

    RelayLeafStats st{};
    int32_t rc = g_get_stats(handle_, &st);
    if (rc != 0) {
      throw JsError(env, "relay_leaf_get_stats failed: " + std::to_string(rc) + " (" + ErrorMessage(rc) + ")");
    }

    // Copy strings now (they are owned by st and must be freed by relay_leaf_free_stats)
    std::string last_error = st.last_error ? st.last_error : "";
    std::string exit_points_json = st.exit_points_json ? st.exit_points_json : "";
    std::string node_addresses_json = st.node_addresses_json ? st.node_addresses_json : "";

    // Free the struct-owned strings
    g_free_stats(&st);

    Napi::Object o = Napi::Object::New(env);
    o.Set("uptimeSeconds", Napi::BigInt::New(env, (int64_t)st.uptime_seconds));
    o.Set("totalStreams",  Napi::BigInt::New(env, (int64_t)st.total_streams));
    o.Set("bytesSent",     Napi::BigInt::New(env, (int64_t)st.bytes_sent));
    o.Set("bytesReceived", Napi::BigInt::New(env, (int64_t)st.bytes_received));
    o.Set("reconnectCount",Napi::BigInt::New(env, (int64_t)st.reconnect_count));

    o.Set("activeStreams", Napi::Number::New(env, st.active_streams));
    o.Set("connectedNodes",Napi::Number::New(env, st.connected_nodes));
    o.Set("connected",     Napi::Boolean::New(env, st.connected));

    o.Set("lastError",      Napi::String::New(env, last_error));
    o.Set("exitPointsJson", Napi::String::New(env, exit_points_json));
    o.Set("nodeAddressesJson", Napi::String::New(env, node_addresses_json));
    return o;
  }
};

// ============================================================
// Module exports
// ============================================================

static Napi::Value Version(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  EnsureLoaded();
  char* p = g_version();
  std::string s = OwnedCStringToStdStringAndFree(p);
  return Napi::String::New(env, s);
}

static Napi::Value ErrorMessageJs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  EnsureLoaded();
  if (info.Length() < 1 || !info[0].IsNumber()) {
    throw JsError(env, "errorMessage(code) requires a number");
  }
  int32_t code = info[0].As<Napi::Number>().Int32Value();
  return Napi::String::New(env, ErrorMessage(code));
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("RelayClient", RelayClient::GetClass(env));
  exports.Set("version", Napi::Function::New(env, Version));
  exports.Set("errorMessage", Napi::Function::New(env, ErrorMessageJs));
  return exports;
}

NODE_API_MODULE(relay_leaf, Init)
