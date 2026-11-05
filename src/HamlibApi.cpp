#include "HamlibApi.h"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace hl {

#ifdef _WIN32
static std::wstring widen(const std::string &s)
{
    if (s.empty())
        return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), &w[0], n);
    return w;
}

static void *openLib(const std::string &path, std::string *why)
{
    const std::wstring w = widen(path);
    const bool absolute = path.find('\\') != std::string::npos || path.find('/') != std::string::npos;
    // With a full path, LOAD_WITH_ALTERED_SEARCH_PATH makes Windows also look for the DLLs that
    // libhamlib-4.dll needs (libusb-1.0.dll, libwinpthread-1.dll…) in the folder of libhamlib-4.dll.
    HMODULE h = LoadLibraryExW(w.c_str(), nullptr, absolute ? LOAD_WITH_ALTERED_SEARCH_PATH : 0);
    if (!h && why) {
        const DWORD err = GetLastError();
        *why = path + ": Windows error " + std::to_string(err)
               + (err == 126 ? " (file or one of its DLLs not found)" : err == 193 ? " (32/64-bit mismatch)" : "");
    }
    return reinterpret_cast<void *>(h);
}

static void *symbol(void *h, const char *name)
{
    return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(h), name));
}

static void closeLib(void *h)
{
    FreeLibrary(reinterpret_cast<HMODULE>(h));
}
#else
static void *openLib(const std::string &path, std::string *why)
{
    void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h && why) {
        const char *e = dlerror();
        *why = e ? e : path;
    }
    return h;
}

static void *symbol(void *h, const char *name)
{
    return dlsym(h, name);
}

static void closeLib(void *h)
{
    dlclose(h);
}
#endif

std::vector<std::string> Api::defaultNames()
{
#if defined(_WIN32)
    return {"libhamlib-4.dll"};
#elif defined(__APPLE__)
    return {"libhamlib.4.dylib", "/opt/homebrew/lib/libhamlib.4.dylib", "/usr/local/lib/libhamlib.4.dylib"};
#else
    return {"libhamlib.so.4", "libhamlib.so"};
#endif
}

void Api::unload()
{
    void *h = m_handle;
    *this = Api{};     // all pointers back to nullptr (m_handle too)
    if (h)
        closeLib(h);
}

bool Api::load(const std::vector<std::string> &candidates, std::string *error)
{
    std::string why;
    for (const std::string &c : candidates) {
        if (c.empty())
            continue;
        if (m_handle && c == m_path)
            return true;                                   // already loaded
        void *h = openLib(c, &why);
        if (!h)
            continue;

        Api a;
        bool ok = true;
        auto get = [&](auto &fn, const char *name, bool required) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(symbol(h, name));
            if (!fn && required) {
                ok = false;
                why = c + ": " + name + " missing (not a Hamlib 4 library?)";
            }
        };
        get(a.rig_init, "rig_init", true);
        get(a.rig_open, "rig_open", true);
        get(a.rig_close, "rig_close", true);
        get(a.rig_cleanup, "rig_cleanup", true);
        get(a.rig_set_conf, "rig_set_conf", true);
        get(a.rig_token_lookup, "rig_token_lookup", true);
        get(a.rig_get_freq, "rig_get_freq", true);
        get(a.rig_set_freq, "rig_set_freq", true);
        get(a.rig_get_mode, "rig_get_mode", true);
        get(a.rig_strrmode, "rig_strrmode", true);
        get(a.rig_set_debug, "rig_set_debug", true);
        get(a.rig_load_all_backends, "rig_load_all_backends", true);
        get(a.rig_list_foreach, "rig_list_foreach", true);
        get(a.rig_get_caps_int, "rig_get_caps_int", false);
        get(a.rig_get_caps_cptr, "rig_get_caps_cptr", false);
        a.version = static_cast<const char *>(symbol(h, "hamlib_version"));
        if (!ok) {
            closeLib(h);
            continue;
        }
        unload();
        *this = a;
        m_handle = h;
        m_path = c;
        return true;
    }
    if (error)
        *error = why.empty() ? std::string("Hamlib library not found") : why;
    return false;
}

}   // namespace hl
