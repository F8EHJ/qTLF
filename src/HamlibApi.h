#pragma once

// Run-time binding to the Hamlib library (no Hamlib SDK or headers needed to build qTLF):
//   Windows  libhamlib-4.dll       (hamlib-w64 / hamlib-w32 from https://github.com/Hamlib/Hamlib/releases)
//   Linux    libhamlib.so.4        (package libhamlib4 / libhamlib4t64)
//   macOS    libhamlib.4.dylib
//
// Only Hamlib's public functions are used, never its structures (they change between versions),
// so any Hamlib 4.x works. This file does not depend on Qt (it can be tested on its own).

#include <cstdint>
#include <type_traits>
#include <string>
#include <vector>

namespace hl {

// Hamlib 4.x C types (ABI)
struct Rig;                         // RIG, opaque
typedef uint32_t rig_model_t;
typedef double freq_t;
typedef uint64_t rmode_t;
typedef long pbwidth_t;             // shortfreq_t ("long": 32 bits on Windows, like the DLL)
typedef unsigned int vfo_t;
typedef long token_t;

constexpr vfo_t VFO_CURR = 1u << 29;     // RIG_VFO_CURR
constexpr int OK = 0;
constexpr int E_TIMEOUT = 5, E_IO = 6, E_PROTO = 8;   // returned negated
constexpr int PORT_SERIAL = 1, PORT_NETWORK = 2, PORT_UDP_NETWORK = 10;
constexpr int CAPS_PORT_TYPE = 2;                      // enum rig_caps_int_e
constexpr int CAPS_MFG_NAME = 1, CAPS_MODEL_NAME = 2, CAPS_STATUS = 3;   // enum rig_caps_cptr_e

struct Api {
    // functions (nullptr until load() succeeds)
    Rig *(*rig_init)(rig_model_t) = nullptr;
    int (*rig_open)(Rig *) = nullptr;
    int (*rig_close)(Rig *) = nullptr;
    int (*rig_cleanup)(Rig *) = nullptr;
    int (*rig_set_conf)(Rig *, token_t, const char *) = nullptr;
    token_t (*rig_token_lookup)(Rig *, const char *) = nullptr;
    int (*rig_get_freq)(Rig *, vfo_t, freq_t *) = nullptr;
    int (*rig_set_freq)(Rig *, vfo_t, freq_t) = nullptr;
    int (*rig_get_mode)(Rig *, vfo_t, rmode_t *, pbwidth_t *) = nullptr;
    const char *(*rig_strrmode)(rmode_t) = nullptr;
    void (*rig_set_debug)(int) = nullptr;
    int (*rig_load_all_backends)() = nullptr;
    int (*rig_list_foreach)(int (*)(const void *caps, void *data), void *data) = nullptr;
    // optional (Hamlib >= 4.2): rig names without reading Hamlib's structures
    long long (*rig_get_caps_int)(rig_model_t, int) = nullptr;
    const char *(*rig_get_caps_cptr)(rig_model_t, int) = nullptr;
    const char *version = nullptr;      // exported variable hamlib_version

    // Loads the first library that can be opened among the candidates (file names or full
    // paths, UTF-8). Unloads a library loaded before from another path.
    bool load(const std::vector<std::string> &candidates, std::string *error);
    void unload();
    bool loaded() const { return m_handle != nullptr; }
    const std::string &path() const { return m_path; }

    // Default candidates for this system (the program folder first on Windows).
    static std::vector<std::string> defaultNames();

    // no destructor: the library stays loaded until the program ends (a plain struct, copyable)

private:
    void *m_handle = nullptr;
    std::string m_path;
};

}   // namespace hl
