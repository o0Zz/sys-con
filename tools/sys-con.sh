#!/usr/bin/env bash
#
# Developer helper for sys-con on a modded Nintendo Switch.
#
# Dependencies: curl, lftp (pacman -S lftp), and devkitPro for `stacktrace`.
# You need sys-ftpd installed on the console.
#
# Boot the console, then:
#   ./tools/sys-con.sh ftp upload   push the sysmodule you just built onto the console
#   ./tools/sys-con.sh ftp logs     download the sysmodule log and colourise it
#   ./tools/sys-con.sh ftp crash    pull the latest Atmosphere fatal report and symbolise it
#   ./tools/sys-con.sh ftp dmp      pull the HID dumps off the console
#
# The same four commands work with `sd` instead of `ftp`, reading and writing the SD card
# directly. That is what you need when the console crashes on boot and never brings sys-ftpd
# up: pull the card, plug it into your laptop, and debug from there.
#
# Configuration -- override in your environment, no need to edit this file:
#   SYSCON_FTP_URL       ftp://192.168.10.238:5000
#   SYSCON_FTP_USER      (empty)
#   SYSCON_FTP_PASS      (empty)
#   SYSCON_SD_MOUNT      /d          where the console's SD card is mounted
#   AFE_PARSER_VERSION   v1.3.1      release of the fatal-error parser to fetch

set -euo pipefail
shopt -s nullglob

# Works from any directory: the script lives in tools/, the repository is its parent.
REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

FTP_URL="${SYSCON_FTP_URL:-ftp://192.168.10.238:5000}"
FTP_USER="${SYSCON_FTP_USER:-}"
FTP_PASS="${SYSCON_FTP_PASS:-}"
SD_MOUNT="${SYSCON_SD_MOUNT:-/d}"
AFE_PARSER_VERSION="${AFE_PARSER_VERSION:-v1.3.1}"

die() { echo "error: $*" >&2; exit 1; }

# Everything below is derived and should not need editing.
#
# The title ID comes from the Makefile rather than being repeated here, so the console-side
# paths built below cannot drift from the tree `make all` actually lays out. Ask make for it
# when make is on PATH; otherwise read the literal out of the Makefile, so that this script
# still runs from a bare shell that has no build toolchain.
read_title_id() {
    local from_make=""
    if command -v make >/dev/null 2>&1; then
        from_make="$(make -s -C "$REPO_ROOT" print-title-id 2>/dev/null || true)"
    fi
    if [ -n "$from_make" ]; then
        printf '%s\n' "$from_make"
        return 0
    fi
    sed -n 's/^TITLE_ID[[:space:]]*:=[[:space:]]*\([0-9A-Fa-f]\{16\}\).*/\1/p' "$REPO_ROOT/Makefile"
}

TITLE_ID="$(read_title_id | tr -d '\r')"
[ -n "$TITLE_ID" ] || die "could not determine TITLE_ID from $REPO_ROOT/Makefile"

ELF_FILE="$REPO_ROOT/src/app/build/sys-con.elf"
NSP_FILE="$REPO_ROOT/out/atmosphere/contents/$TITLE_ID/exefs.nsp"
NRO_FILE="$REPO_ROOT/out/switch/sys-con.nro"

DST_NSP_FILE="atmosphere/contents/$TITLE_ID/exefs.nsp"
DST_NRO_FILE="switch/sys-con.nro"
LOG_FILE="config/sys-con/log.txt"
REPORT_FOLDER="atmosphere/fatal_errors"
DMP_FOLDER="config/sys-con"

# Artefacts pulled off the console land here, not in the repository root. Keeping them in
# one gitignored directory is what lets the cleanup globs below use `rm -f` safely.
DEBUG_DIR="$REPO_ROOT/debug"
AFE_PARSER="$REPO_ROOT/tools/AFE_Parser.exe"

usage() {
    cat >&2 <<USAGE
Usage:
    $0 ftp <upload|logs|crash|dmp>   over the network, via sys-ftpd
    $0 sd  <upload|logs|crash|dmp>   directly on the SD card at $SD_MOUNT
    $0 build
    $0 stacktrace
USAGE
    exit 1
}

# --- Transport ---------------------------------------------------------------------------
# Three primitives, implemented once per transport. Every command is written against these,
# so the ftp and sd paths cannot drift apart the way they had: `ftp dmp` used to delete the
# dumps from the console while `sd dmp` left them on the card.

TRANSPORT=""

ftp_put() { curl --fail -u "$FTP_USER:$FTP_PASS" -T "$1" "$FTP_URL/$2"; }
ftp_get() { curl --fail -u "$FTP_USER:$FTP_PASS" "$FTP_URL/$1" -o "$2"; }

ftp_mget() { # <remote-dir> <glob> <local-dir> <delete 0|1>
    local remote_dir=$1 glob=$2 local_dir=$3 delete=$4 delete_cmd=""
    command -v lftp >/dev/null 2>&1 || die "lftp is not installed (pacman -S lftp)"
    if [ "$delete" = 1 ]; then delete_cmd="glob -a rm $glob"; fi
    # lftp exits non-zero when the glob matches nothing, which is the normal case for crash
    # reports on a healthy console. Callers check what actually landed in $local_dir instead.
    lftp -u "$FTP_USER","$FTP_PASS" "$FTP_URL" <<LFTP || true
cd $remote_dir
mget $glob -O $local_dir
$delete_cmd
bye
LFTP
}

sd_require() {
    [ -d "$SD_MOUNT" ] || die "SD card not found at $SD_MOUNT (set SYSCON_SD_MOUNT)"
}

sd_put() {
    sd_require
    cp "$1" "$SD_MOUNT/$2"
    if command -v sync >/dev/null 2>&1; then sync; fi
}

sd_get() {
    sd_require
    cp "$SD_MOUNT/$1" "$2"
}

sd_mget() { # <remote-dir> <glob> <local-dir> <delete 0|1>
    local remote_dir=$1 glob=$2 local_dir=$3 delete=$4
    sd_require
    local matches=( "$SD_MOUNT/$remote_dir"/$glob )
    if [ ${#matches[@]} -eq 0 ]; then return 0; fi
    cp "${matches[@]}" "$local_dir"
    if [ "$delete" = 1 ]; then rm -f "${matches[@]}"; fi
}

transport_put()  { "${TRANSPORT}_put"  "$@"; }
transport_get()  { "${TRANSPORT}_get"  "$@"; }
transport_mget() { "${TRANSPORT}_mget" "$@"; }

# --- Display -----------------------------------------------------------------------------

display_logs() {
    echo "---- LOGS ----"
    local line
    while IFS= read -r line; do
        case $line in
            "|I|"*) printf '\033[0;34m%s\033[0m\n' "$line" ;;
            "|E|"*) printf '\033[0;31m%s\033[0m\n' "$line" ;;
            "|W|"*) printf '\033[0;33m%s\033[0m\n' "$line" ;;
            *)      printf '%s\n' "$line" ;;
        esac
    done < "$1"
    echo "---- END ----"
}

fetch_afe_parser() {
    if [ -f "$AFE_PARSER" ]; then return 0; fi
    echo "Fetching AFE_Parser $AFE_PARSER_VERSION ..."
    # --fail so that an HTTP error page is not saved as a .exe and then run.
    curl --fail -L -o "$AFE_PARSER" \
        "https://github.com/o0Zz/AFE_Parser/releases/download/$AFE_PARSER_VERSION/AFE_Parser.exe" \
        || die "could not download AFE_Parser $AFE_PARSER_VERSION"
    chmod +x "$AFE_PARSER"
}

display_fatalerror() {
    local reports=( "$DEBUG_DIR"/*.bin )
    if [ ${#reports[@]} -eq 0 ]; then
        echo "*** No crash report ***"
        exit 1
    fi
    [ -f "$ELF_FILE" ] || die "$ELF_FILE not found -- the report cannot be symbolised without it"
    fetch_afe_parser
    "$AFE_PARSER" -report "${reports[0]}" -elf "$ELF_FILE"
}

# --- Commands ----------------------------------------------------------------------------

cmd_upload() {
    [ -f "$NSP_FILE" ] || die "$NSP_FILE not found -- run 'make all' first"
    echo "Uploading $DST_NSP_FILE"
    transport_put "$NSP_FILE" "$DST_NSP_FILE"
    #transport_put "$NRO_FILE" "$DST_NRO_FILE"
    echo "OK"
}

cmd_logs() {
    mkdir -p "$DEBUG_DIR"
    transport_get "$LOG_FILE" "$DEBUG_DIR/log.txt"
    display_logs "$DEBUG_DIR/log.txt"
}

cmd_crash() {
    mkdir -p "$DEBUG_DIR"
    rm -f "$DEBUG_DIR"/*.bin
    echo "Downloading all .bin crash reports from $REPORT_FOLDER ..."
    transport_mget "$REPORT_FOLDER" '*.bin' "$DEBUG_DIR" 1
    display_fatalerror
}

cmd_dmp() {
    mkdir -p "$DEBUG_DIR"
    rm -f "$DEBUG_DIR"/*.dmp
    echo "Downloading and deleting all .dmp files from $DMP_FOLDER ..."
    transport_mget "$DMP_FOLDER" '*.dmp' "$DEBUG_DIR" 1
    local dumps=( "$DEBUG_DIR"/*.dmp )
    if [ ${#dumps[@]} -eq 0 ]; then
        echo "*** No dump found ***"
    else
        printf '%s\n' "${dumps[@]}"
    fi
}

cmd_build() {
    rm -f "$REPO_ROOT"/*.zip
    make -C "$REPO_ROOT" distclean ATMOSPHERE=0
}

find_addr2line() {
    local base="${DEVKITPRO:-/opt/devkitpro}/devkitA64/bin/aarch64-none-elf-addr2line" candidate
    for candidate in "$base" "$base.exe"; do
        if [ -x "$candidate" ]; then printf '%s\n' "$candidate"; return 0; fi
    done
    if command -v aarch64-none-elf-addr2line >/dev/null 2>&1; then
        command -v aarch64-none-elf-addr2line
        return 0
    fi
    die "aarch64-none-elf-addr2line not found (set DEVKITPRO)"
}

cmd_stacktrace() {
    [ -f "$ELF_FILE" ] || die "$ELF_FILE not found -- build first"
    local addr2line pc_value backtrace_value addr
    addr2line="$(find_addr2line)"
    read -r -p "PC value (hex, without 0x): " pc_value
    read -r -p "Backtrace start address (hex, without 0x): " backtrace_value
    addr=$(printf "0x%X" $(( 0x$pc_value - 0x$backtrace_value )))
    "$addr2line" -e "$ELF_FILE" -f -p -C -a "$addr"
}

# --- Entry point -------------------------------------------------------------------------

main() {
    case ${1:-} in
        ftp|sd)
            TRANSPORT=$1
            case ${2:-} in
                upload) cmd_upload ;;
                logs)   cmd_logs ;;
                crash)  cmd_crash ;;
                dmp)    cmd_dmp ;;
                *)      usage ;;
            esac
            ;;
        build)      cmd_build ;;
        stacktrace) cmd_stacktrace ;;
        *)          usage ;;
    esac
}

main "$@"
