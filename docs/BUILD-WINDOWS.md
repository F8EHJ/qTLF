# Building and deploying qTLF on Windows

## 1. Tools

* **Qt 6** (6.2 or newer) with the online installer (https://www.qt.io/download-qt-installer),
  kit **MinGW 64-bit**, plus the *Qt Quick* / *Qt Declarative* modules (default) and the
  *MinGW* compiler and *CMake* from "Developer and Designer Tools".
* **Hamlib for Windows**, for TRX control: `hamlib-w64-4.x.x.zip` from
  https://github.com/Hamlib/Hamlib/releases (e.g. `hamlib-w64-4.5.5.zip`), unzipped for example to
  `C:\hamlib-w64-4.5.5`. A 64-bit qTLF needs the **w64** version (the w32 DLL gives
  "Windows error 193").

No Hamlib SDK is needed to *build* qTLF: `libhamlib-4.dll` is loaded when qTLF starts.

## 2. Build + deploy (one command)

**CMake missing?** (`ERROR: CMake not found`) Install it once, then run the script again in a new console:

* `C:\Qt\MaintenanceTool.exe` › *Add or remove components* › Qt › *Developer and Designer Tools* › tick **CMake** and **Ninja**; or
* `winget install Kitware.CMake Ninja-build.Ninja`; or
* https://cmake.org/download — *Windows x64 Installer*, option "Add CMake to the PATH".

In the qTLF folder, run (from any console, or by double-click without the Hamlib argument):

```bat
deploy-windows.bat C:\hamlib-w64-4.5.5\bin
```

The script finds Qt (`C:\Qt\6.x\mingw_64`), the MinGW compiler (`C:\Qt\Tools\mingw*_64`), CMake
(`C:\Qt\Tools\CMake*`, `C:\Program Files\CMake`, Visual Studio…) and Ninja (`C:\Qt\Tools\Ninja`) by itself, even when they are not in the
PATH (the "Qt 6.x (MinGW 64-bit)" console does not add CMake). Qt is found in `X:\Qt` on any drive (C:, D:, G:…); another folder: `set QT_ROOT=D:\MyQt` first.
It shows the tools found, then `[1/3] Configure`, `[2/3] Compile`, `[3/3] Deploy`, and on error which
step failed and why.

Result: `build-win\qtlf-deploy\` with `qtlf.exe`, the Qt DLLs, the QML modules, Qt's translations
(`translations\qtbase_fr.qm`… for the OK / Cancel buttons), `rules\` and the
Hamlib DLLs (`libhamlib-4.dll`, `libusb-1.0.dll`, `libwinpthread-1.dll`, `libgcc_s_*.dll`).
Zip that folder: it runs on any Windows 10/11 PC without installing anything.

Without the argument the Hamlib DLLs are not copied (qTLF works, TRX control says the DLL is missing).

Manual equivalent:

```bat
cmake -S . -B build-win -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DQTLF_HAMLIB_BIN=C:\hamlib-w64-4.5.5\bin
cmake --build build-win
cmake --build build-win --target deploy_qtlf
```

In Qt Creator: open `CMakeLists.txt`, set `QTLF_HAMLIB_BIN` in *Projects › Build › CMake*, and build
the target **deploy_qtlf**.

## 3. TRX control (Hamlib) on Windows

qTLF looks for `libhamlib-4.dll`, in this order:

1. the file set in *Configuration › TRX › Hamlib library* (INI `[trx] hamlib_library`);
2. next to `qtlf.exe` (what `deploy-windows.bat` does);
3. `C:\Program Files\hamlib*\bin\`, `C:\hamlib*\bin\`, `%USERPROFILE%\hamlib*\bin\` (newest first);
4. the folders of the `PATH`.

The DLLs that `libhamlib-4.dll` needs are searched in its own folder, so pointing the setting to
`C:\hamlib-w64-4.5.5\bin\libhamlib-4.dll` is enough.

Settings (*Configuration › TRX control…*):

| Field | Example |
|---|---|
| Rig model (RIGMODEL) | `3073` Icom IC-7300, `1035` Yaesu FT-991 — type part of the name to search |
| Speed (RIGSPEED) | `115200` (IC-7300 USB), must match the rig menu (CI-V / CAT rate) |
| Port (RIGPORT) | `COM5` — see *Device Manager › Ports (COM & LPT)*; the list shows the ports present |

The tab shows `Hamlib 4.5.5 — 274 rigs — <file>` when the DLL is loaded, otherwise the reason.
**Test connection** reads the frequency once.

Check with Hamlib alone first (same folder): `rigctl -m 3073 -s 115200 -r COM5 f`

* **Only one program can open a COM port.** To share the rig with WSJT-X, N1MM, fldigi…: run
  `rigctld -m 3073 -s 115200 -r COM5` and set every program, qTLF included, to model **2
  "NET rigctl"**, port `localhost:4532`.
* Icom: USB driver from Icom (Silicon Labs CP210x), *CI-V USB Echo Back* OFF, *CI-V Transceive* ON.
* Hamlib traces: `set QTLF_HAMLIB_DEBUG=5` then start `qtlf.exe` from that console.

## 4. Configuration and data

* Configuration: `%APPDATA%\qTLF\qTLF.ini`
* Logs database: `%LOCALAPPDATA%\qTLF\qTLF\qtlf.sqlite`
* Contests: `rules\` next to `qtlf.exe`
