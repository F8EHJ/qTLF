# Building qTLF on Linux (without Qt Creator)

Requirements: CMake ≥ 3.16, a C++17 compiler (g++ ≥ 9), Qt ≥ 6.2 with Widgets, Sql (+ SQLite
driver), Qml, Quick, QuickWidgets, Network, and the QtQuick / QtQuick.Layouts QML modules.
Tested on Ubuntu 24.04 (Qt 6.4). Ubuntu 22.04 (Qt 6.2), Debian 12 and newer work the same way.

## Quick way: the script

```sh
unzip qTLF-x.y.z.zip
cd qTLF
./build-linux.sh --deps      # first time: installs the packages (sudo), builds, installs in ~/.local
./build-linux.sh             # later: rebuild + reinstall (no root needed)
```

Options: `--prefix /usr/local` (system-wide, uses sudo), `--no-install` (build only),
`--clean` (fresh build folder), `--debug`.

## Step by step

### 1. Packages

Debian / Ubuntu / Linux Mint / Raspberry Pi OS (bookworm):

```sh
sudo apt update
sudo apt install build-essential cmake libhamlib-utils qt6-l10n-tools qt6-tools-dev \
     qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
     qml6-module-qtquick qml6-module-qtquick-layouts \
     qml6-module-qtquick-window qml6-module-qtqml-workerscript
```

Other distributions (package names may vary slightly):

| Distribution | Command |
|---|---|
| Fedora | `sudo dnf install gcc-c++ make cmake hamlib qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel` |
| Arch / Manjaro | `sudo pacman -S --needed base-devel cmake hamlib qt6-base qt6-declarative qt6-tools` |
| openSUSE | `sudo zypper install gcc-c++ make cmake hamlib qt6-base-devel qt6-declarative-devel qt6-sql-sqlite qt6-tools-linguist` |

`qt6-l10n-tools` + `qt6-tools-dev` (Qt Linguist tools) build the translations of the user interface
(French…) into the program; without them CMake prints a warning and qTLF is in English only.
`qt6-translations-l10n` (usually installed) translates Qt's own buttons and dialogs.

Hamlib (≥ 4.0) is only needed **at run time**, for *TRX control*: qTLF loads `libhamlib.so.4`
when it starts (no Hamlib headers needed to build). `libhamlib-utils` installs it with `rigctl` /
`rigctld` (useful to test the rig). Without it qTLF works, TRX control says the library is missing.
`-DQTLF_WITH_HAMLIB=OFF` removes TRX control from the build.

Check the Qt version: `qmake6 --version` or `qtpaths6 --qt-version` (6.2 or newer).

### 2. Configure and build

```sh
cd qTLF
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j"$(nproc)"
```

You can already run it from the source folder: `./build/qtlf`
(it then uses the contests of `qTLF/rules/`).

### 3. Install

```sh
cmake --install build
```

Installs, under `~/.local`:

| File | |
|---|---|
| `bin/qtlf` | the logger |
| `share/qtlf/rules/` | contest definitions. A contest shipped with qTLF (SKM, GENERIC, RENCONTRES-UFT…) is **replaced as a whole** when the new version differs — the old folder is moved to `share/qtlf/rules-backup/<ID>-<date>` (old and new files are never mixed). Your own contest folders are never touched. |
| `share/applications/qtlf.desktop` | menu entry "qTLF" |
| `share/icons/hicolor/*/apps/qtlf.png` | icon |

Start with `~/.local/bin/qtlf` (or `qtlf` if `~/.local/bin` is in your `PATH` — the default on
Ubuntu after a new login), or from the desktop menu.

System-wide instead: `-DCMAKE_INSTALL_PREFIX=/usr/local` at step 2 and `sudo cmake --install build`.
The contests are then in `/usr/local/share/qtlf/rules` (root-owned): downloaded data files go to
`~/.local/share/qTLF/qTLF/rules/` automatically.

### 4. Update to a new version

```sh
cd qTLF            # new sources
./build-linux.sh --clean
```

Your log database, configuration and own contests are kept; the contests shipped with qTLF are
updated (previous copies in `~/.local/share/qtlf/rules-backup/`).

## Where qTLF keeps its files

| What | Where |
|---|---|
| Configuration (station, keyer, CW messages, TRX) | `~/.config/qTLF/qTLF.ini` |
| Log database (all contests) | `~/.local/share/qTLF/qTLF/qtlf.sqlite` |
| Contests | `~/.local/share/qtlf/rules/` (installed) or `qTLF/rules/` (run from `build/`) |
| Data files downloaded from the network | `~/.local/share/qTLF/qTLF/rules/<ID>/` |

The paths can be changed in Configuration › Paths; `QTLF_RULES=/path/to/rules qtlf` forces the
contest folder for one run.

## CW keyer: cwdaemon

```sh
sudo apt install cwdaemon
```

Start it with your interface (see `man cwdaemon`), then in qTLF: Configuration › Keyer › type
*cwdaemon*, address `localhost`, port `6789`. Without a radio, Wireshark (filter `udp.port == 6789`,
loopback interface) shows what qTLF sends.

## TRX control: Hamlib

qTLF reads the rig frequency (and mode) through Hamlib, the library behind `rigctl`.
In qTLF: **Configuration › TRX control…**

| Field | INI key (`[trx]`) | Same as | Example |
|---|---|---|---|
| TRX control | `type` | — | `hamlib` (or `none`) |
| Rig model (RIGMODEL) | `rig_model` | `rigctl -m` | `3073` (Icom IC-7300), `1035` (Yaesu FT-991) — type part of the name to search the list |
| Speed (RIGSPEED) | `rig_speed` | `rigctl -s` | `57600`, `38400`… — must match the CAT/CI-V speed in the rig's menu |
| Port (RIGPORT) | `rig_port` | `rigctl -r` | `/dev/ttyUSB0`, or better `/dev/serial/by-id/usb-Silicon_Labs_…` (does not change between reboots) |
| Poll interval | `poll_ms` | — | `500` ms |
| QSY the rig | `set_freq` | — | `true`: a frequency typed in qTLF (Freq field, `7025` in Call) tunes the rig |
| Follow mode | `follow_mode` | — | `true`: rig CW → qTLF CW, USB/LSB → SSB… if the contest has that mode |

*Hamlib library*: empty = automatic (`libhamlib.so.4`), or the full path of another Hamlib build.
The tab shows the Hamlib version and the number of rigs when the library is loaded.

**Test connection** opens the rig, reads the frequency and closes it. The status bar shows
`TRX: ● rig port` (green = connected, red = error, hover for the reason); qTLF retries every 5 s,
so the rig can be switched on after qTLF.

Check first with Hamlib alone (the rig must answer here before qTLF can work):

```sh
rigctl -l | grep -i 7300                    # find RIGMODEL
rigctl -m 3073 -s 57600 -r /dev/ttyUSB0 f   # prints the frequency in Hz
```

* **Permission denied** on `/dev/ttyUSB0`: `sudo usermod -aG dialout $USER`, then log out and in.
* **Timeout / protocol error**: wrong RIGSPEED, wrong RIGMODEL, or CAT disabled in the rig menu
  (Icom: *CI-V USB baud rate*, *CI-V transceive*; Yaesu: *CAT RATE*).
* **Sharing the rig** with other programs (WSJT-X, fldigi…): only one program can open a serial port.
  Run `rigctld -m 3073 -s 57600 -r /dev/ttyUSB0 &` and set every program, qTLF included, to
  model **2 "NET rigctl"**, port `localhost:4532`.
* Hamlib traces in the terminal: `QTLF_HAMLIB_DEBUG=5 qtlf`.

## Uninstall / start again from a clean state

```sh
cd qTLF
./uninstall-linux.sh          # program, menu entry, icons, installed contests, downloaded files,
                              # build/ folder — KEEPS qTLF.ini and the log database
./uninstall-linux.sh --all    # everything, configuration and database included
                              # (the database is first copied to ~/qtlf-backup-<date>.sqlite)
./build-linux.sh              # reinstall
```

The script lists what it will delete and asks before doing it (`--yes` to skip the question,
`--prefix /usr/local` for a system-wide install). If something still looks old after that:

* `which -a qtlf` — another qtlf in the PATH (other prefix, old copy in `/usr/local/bin`)?
* Configuration › Paths › *Rules folder* must be empty (automatic), and `echo $QTLF_RULES` empty:
  both force an old rules folder.
* Help › About shows the version and the rules folder actually used.

## Troubleshooting

| Message | Fix |
|---|---|
| `Could not find a package configuration file provided by "Qt6"` | Qt 6 dev packages missing: step 1. Several Qt versions: add `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64` |
| `Could NOT find WrapOpenGL` / `OpenGL` | `sudo apt install libgl-dev` |
| `Driver not loaded` / database error at start | `sudo apt install libqt6sql6-sqlite` |
| Score / Silent Keys windows empty, `module "QtQuick" is not installed` in the terminal | `sudo apt install qml6-module-qtquick qml6-module-qtquick-layouts qml6-module-qtqml-workerscript` |
| `qt.qpa.plugin: Could not load the Qt platform plugin "xcb"` | `sudo apt install libxcb-cursor0` |
| TRX: "Hamlib library not found" | `sudo apt install libhamlib-utils` (or set the library file in Configuration › TRX) |
