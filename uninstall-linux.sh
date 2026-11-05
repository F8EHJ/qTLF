#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  qTLF — remove everything to start again from a clean state (Linux).
#
#    ./uninstall-linux.sh           remove the program, menu entry, icons, installed contests,
#                                   downloaded data files and the build folder.
#                                   KEEPS your configuration (qTLF.ini) and your logs (database).
#    ./uninstall-linux.sh --all     also removes the configuration and the database.
#                                   The database is first copied to ~/qtlf-backup-<date>.sqlite
#    ./uninstall-linux.sh --prefix /usr/local   if qTLF was installed with that prefix (sudo)
#    ./uninstall-linux.sh --yes     do not ask for confirmation
# ---------------------------------------------------------------------------
set -euo pipefail

PREFIX="$HOME/.local"
ALL=0
YES=0
while [ $# -gt 0 ]; do
    case "$1" in
        --all)       ALL=1 ;;
        --yes|-y)    YES=1 ;;
        --prefix)    PREFIX="$2"; shift ;;
        --prefix=*)  PREFIX="${1#*=}" ;;
        -h|--help)   sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "Unknown option $1 (see --help)"; exit 1 ;;
    esac
    shift
done

SRC="$(cd "$(dirname "$0")" && pwd)"
CFG="${XDG_CONFIG_HOME:-$HOME/.config}/qTLF"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/qTLF"

# files and folders to remove
TARGETS=(
    "$PREFIX/bin/qtlf"
    "$PREFIX/bin/qtlf-udpmon"                           # test tool of older versions
    "$PREFIX/share/qtlf"                                # installed contests + rules-backup
    "$PREFIX/share/applications/qtlf.desktop"
)
for s in 16 24 32 48 64 128 256 512; do
    TARGETS+=("$PREFIX/share/icons/hicolor/${s}x${s}/apps/qtlf.png")
done
TARGETS+=("$DATA/qTLF/rules")                           # data files downloaded from the network
[ -d "$SRC/build" ] && TARGETS+=("$SRC/build")          # build folder of this source tree
if [ "$ALL" = 1 ]; then
    TARGETS+=("$CFG" "$DATA")
fi

echo "qTLF clean-up — these will be deleted:"
found=0
for t in "${TARGETS[@]}"; do
    if [ -e "$t" ]; then echo "  $t"; found=1; fi
done
if [ "$found" = 0 ]; then echo "  (nothing found — already clean)"; exit 0; fi
if [ "$ALL" = 0 ]; then
    echo "Kept: configuration $CFG/qTLF.ini and logs $DATA/qTLF/qtlf.sqlite  (use --all to remove them too)"
fi

if [ "$YES" = 0 ]; then
    read -r -p "Continue? [y/N] " a
    case "$a" in y|Y|o|O|yes|oui) ;; *) echo "Cancelled."; exit 1 ;; esac
fi

pkill -x qtlf 2>/dev/null || true                       # a running qTLF keeps files open

if [ "$ALL" = 1 ]; then
    for db in "$DATA"/qTLF/*.sqlite; do
        [ -f "$db" ] || continue
        b="$HOME/qtlf-backup-$(date +%Y%m%d-%H%M%S)-$(basename "$db")"
        cp -p "$db" "$b"
        echo "Database saved to $b"
    done
fi

SUDO=""
if [ -e "$PREFIX" ] && [ ! -w "$PREFIX" ]; then SUDO="sudo"; fi
for t in "${TARGETS[@]}"; do
    [ -e "$t" ] || continue
    case "$t" in
        "$PREFIX"/*) $SUDO rm -rf -- "$t" ;;
        *)           rm -rf -- "$t" ;;
    esac
done

command -v update-desktop-database >/dev/null && update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
hash -r 2>/dev/null || true

echo "Done. To reinstall:  ./build-linux.sh"
if command -v qtlf >/dev/null 2>&1; then
    echo "WARNING: another qtlf is still in your PATH: $(command -v qtlf)"
    echo "         (installed with another prefix? run this script again with --prefix $(dirname "$(dirname "$(command -v qtlf)")"))"
fi
