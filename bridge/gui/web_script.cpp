// BrassCue - GPL-3.0. Native asynchronous evaluation preserves CSP without unsafe-eval.
#include "web_script.hpp"
#include <cstdint>
#include <memory>
#include <wx/weakref.h>
#ifdef __WXMSW__
#include <WebView2.h>
#include <wrl.h>
#else
#include <cstddef>
#include <dlfcn.h>
#endif
namespace {
struct Completion {
    wxWeakRef<wxWebView> browser;
    void *data;
    void Send(bool success, const wxString &message) const {
        if (!browser)
            return;
        auto *event = new wxWebViewEvent(wxEVT_WEBVIEW_SCRIPT_RESULT, browser->GetId(),
                                         browser->GetCurrentURL(), "");
        event->SetEventObject(browser.get());
        event->SetClientData(data);
        event->SetInt(success ? 1 : 0);
        event->SetString(message);
        wxQueueEvent(browser.get(), event);
    }
};
#ifndef __WXMSW__
// Public WebKit/GLib C ABI. Resolve from the backend already loaded by wxWidgets,
// so a wx build using libsoup 2 (4.0) never loads a second libsoup 3 (4.1) backend.
struct _WebKitWebView;
struct _GAsyncResult;
struct _GCancellable;
struct _GObject;
struct _GError {
    std::uint32_t domain;
    int code;
    char *message;
};
struct _JSCValue;
using Ready = void (*)(_GObject *, _GAsyncResult *, void *);
struct WebKitApi {
    using Evaluate = void (*)(_WebKitWebView *, const char *, std::ptrdiff_t, const char *,
                              const char *, _GCancellable *, Ready, void *);
    using Finish = _JSCValue *(*)(_WebKitWebView *, _GAsyncResult *, _GError **);
    Evaluate evaluate =
        reinterpret_cast<Evaluate>(dlsym(RTLD_DEFAULT, "webkit_web_view_evaluate_javascript"));
    Finish finish =
        reinterpret_cast<Finish>(dlsym(RTLD_DEFAULT, "webkit_web_view_evaluate_javascript_finish"));
    char *(*text)(_JSCValue *) =
        reinterpret_cast<char *(*)(_JSCValue *)>(dlsym(RTLD_DEFAULT, "jsc_value_to_string"));
    void (*unref)(void *) = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_DEFAULT,
                                                                     "g_object_unref"));
    void (*error_free)(_GError *) = reinterpret_cast<void (*)(_GError *)>(dlsym(RTLD_DEFAULT,
                                                                                "g_error_free"));
    void (*free)(void *) = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_DEFAULT, "g_free"));
};
const WebKitApi &Api() {
    static const WebKitApi api;
    return api;
}
void Evaluated(_GObject *object, _GAsyncResult *result, void *context) {
    std::unique_ptr<Completion> completion(static_cast<Completion *>(context));
    const auto &api = Api();
    _GError *error = nullptr;
    auto *value = api.finish(reinterpret_cast<_WebKitWebView *>(object), result, &error);
    if (!value) {
        completion->Send(false, error && error->message ? wxString::FromUTF8(error->message)
                                                        : "Native WebKit script evaluation failed");
        if (error)
            api.error_free(error);
        return;
    }
    auto *text = api.text(value);
    completion->Send(true, text ? wxString::FromUTF8(text) : wxString{});
    api.free(text);
    api.unref(value);
}
#endif
} // namespace
void RunPanelScript(wxWebView &browser, const wxString &script, void *client_data) {
    Completion completion{wxWeakRef<wxWebView>(&browser), client_data};
#ifdef __WXMSW__
    auto *backend = static_cast<ICoreWebView2 *>(browser.GetNativeBackend());
    if (!backend) {
        completion.Send(false, "Edge backend is unavailable");
        return;
    }
    const auto status = backend->ExecuteScript(
        script.wc_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                             [completion](HRESULT status, LPCWSTR result) -> HRESULT {
                                 completion.Send(SUCCEEDED(status),
                                                 SUCCEEDED(status)
                                                     ? wxString(result)
                                                     : "Edge script evaluation failed");
                                 return S_OK;
                             })
                             .Get());
    if (FAILED(status))
        completion.Send(false, "Edge script submission failed");
#else
    const auto &api = Api();
    if (!api.evaluate || !api.finish || !api.text || !api.unref || !api.free || !api.error_free) {
        completion.Send(false,
                        "WebKitGTK 2.40 or newer is required; native controls remain available");
        return;
    }
    api.evaluate(static_cast<_WebKitWebView *>(browser.GetNativeBackend()), script.utf8_str(), -1,
                 nullptr, nullptr, nullptr, Evaluated, new Completion(completion));
#endif
}
