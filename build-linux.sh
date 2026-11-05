#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  qTLF — build and install on Linux, without Qt Creator.
#
#    ./build-linux.sh --deps        install the build dependencies (sudo), then build + install
#    ./build-linux.sh               build + install into ~/.local (no root needed)
#    ./build-linux.sh --prefix /usr/local     (system-wide install, asks for sudo)
#    ./build-linux.sh --no-install  build only (run ./build/qtlf from the source folder)
#    ./build-linux.sh --clean       remove the build folder first
#
#  Installed files (prefix ~/.local):
#    bin/qtlf
#    share/qtlf/rules/...            contests (a changed shipped contest is replaced, old one in rules-backup/)
#    share/applications/qtlf.desktop + share/icons/hicolor/*/apps/qtlf.png  (menu entry)
# ---------------------------------------------------------------------------
set -euo pipefail

PREFIX="$HOME/.local"
DO_DEPS=0
DO_INSTALL=1
DO_CLEAN=0
BUILD_TYPE=Release

while [ $# -gt 0 ]; do
    case "$1" in
        --deps)        DO_DEPS=1 ;;
        --prefix)      PREFIX="$2"; shift ;;
        --prefix=*)    PREFIX="${1#*=}" ;;
        --no-install)  DO_INSTALL=0 ;;
        --clean)       DO_CLEAN=1 ;;
        --debug)       BUILD_TYPE=Debug ;;
        -h|--help)     sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1 (see --help)"; exit 1 ;;
    esac
    shift
done

SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD="$SRC/build"
cd "$SRC"

say() { printf '\n\033[1;32m==> %s\033[0m\n' "$*"; }
die() { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

# ---- 1. dependencies -------------------------------------------------------
if [ "$DO_DEPS" = 1 ]; then
    say "Installing build dependencies"
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" debian "*|*" ubuntu "*)
            sudo apt-get update
            sudo apt-get install -y build-essential cmake libhamlib-utils qt6-l10n-tools qt6-tools-dev \
                qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
                qml6-module-qtquick qml6-module-qtquick-layouts \
                qml6-module-qtquick-window qml6-module-qtqml-workerscript ;;
        *" fedora "*|*" rhel "*)
            sudo dnf install -y gcc-c++ make cmake hamlib qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel ;;
        *" arch "*)
            sudo pacman -S --needed --noconfirm base-devel cmake hamlib qt6-base qt6-declarative qt6-tools ;;
        *" suse "*|*" opensuse "*)
            sudo zypper install -y gcc-c++ make cmake hamlib qt6-base-devel qt6-declarative-devel qt6-tools-linguist \
                qt6-sql-sqlite ;;
        *) die "Unknown distribution '${ID:-?}': install CMake, a C++17 compiler and Qt >= 6.2
       (Widgets, Sql + SQLite driver, Qml, Quick, QuickWidgets, Network) by hand." ;;
    esac
fi

# ---- 2. checks -------------------------------------------------------------
command -v cmake >/dev/null || die "cmake not found (run with --deps)"
command -v c++ >/dev/null || command -v g++ >/dev/null || die "C++ compiler not found (run with --deps)"

# ---- 3. configure + build --------------------------------------------------
[ "$DO_CLEAN" = 1 ] && { say "Removing $BUILD"; rm -rf "$BUILD"; }

say "Configuring ($BUILD_TYPE, install prefix $PREFIX)"
cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    || die "CMake configuration failed — missing Qt 6 packages? Run: $0 --deps"

say "Building with $(nproc) jobs"
cmake --build "$BUILD" -j"$(nproc)"

if [ "$DO_INSTALL" = 0 ]; then
    say "Done. Run:  $BUILD/qtlf"
    exit 0
fi

# ---- 4. install ------------------------------------------------------------
say "Installing into $PREFIX"
if [ -w "$PREFIX" ] || { [ ! -e "$PREFIX" ] && mkdir -p "$PREFIX" 2>/dev/null; }; then
    cmake --install "$BUILD"
else
    sudo cmake --install "$BUILD"
fi

# menu entry / icon cache (optional tools)
command -v update-desktop-database >/dev/null && update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true

say "qTLF installed"
echo "  program : $PREFIX/bin/qtlf"
echo "  contests: $PREFIX/share/qtlf/rules"
echo "  menu    : qTLF (Network / Ham Radio)"
case ":$PATH:" in
    *":$PREFIX/bin:"*) echo "  start   : qtlf" ;;
    *) echo "  start   : $PREFIX/bin/qtlf"
       echo "  (add $PREFIX/bin to your PATH to type just 'qtlf')" ;;
esac
