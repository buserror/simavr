#!/bin/bash
#
# Copied from the avr-gdb repository, where it started; keep the two in step by
# hand. The one difference is --ucrt-ok below.
#
# Reports what a finished client demands of the system it will run on, and fails
# if it demands more than it was allowed to. One script for all three platforms,
# because the question is the same everywhere and only the place to read the
# answer differs:
#
#   ELF     the highest glibc version referenced   readelf -V
#   Mach-O  the deployment target                  vtool -show-build
#   PE      the DLLs and functions it imports      objdump -p
#
# Linux is the only one of the three with no way to declare that floor in the
# build; it falls out of the distribution one builds on. Hence this script: what
# cannot be stated has to be measured.
#
# Reading works across architectures -- readelf parses ELF directly instead of
# going through a target backend -- so an armhf client can be checked on an x86
# machine, which is what the arm job does.
#
# Usage:
#   ./check-floor.sh <binary> <floor> [--armv6-hardfloat]
#
#   ./check-floor.sh tool/avr-gdb 2.35
#   ./check-floor.sh tool/avr-gdb 2.31 --armv6-hardfloat
#   ./check-floor.sh tool/avr-gdb 11.0
#   ./check-floor.sh tool/avr-gdb.exe 6.0
#   ./check-floor.sh bin/simavr.exe 10.0 --ucrt-ok

set -eu

BIN=${1:?usage: check-floor.sh <binary> <floor> [--armv6-hardfloat] [--ucrt-ok]}
FLOOR=${2:?usage: check-floor.sh <binary> <floor> [--armv6-hardfloat] [--ucrt-ok]}
FLAGS=" ${*:3} "

has() { case "$FLAGS" in *" $1 "*) return 0 ;; *) return 1 ;; esac; }

fail() { echo "::error::$*" >&2; echo "FAILED: $*" >&2; exit 1; }

test -f "$BIN" || fail "no such file: $BIN"
echo "=== $BIN"
file "$BIN"
KIND=$(file -b "$BIN")

case "$KIND" in
  *ELF*)
    echo "--- libraries it needs at start ---"
    readelf -d "$BIN" | grep NEEDED || echo "(none -- statically linked)"
    readelf -l "$BIN" | grep -i interpreter || echo "(no interpreter)"

    echo "--- glibc ---"
    MAX=$(readelf -V "$BIN" | grep -o 'GLIBC_2\.[0-9]*' | sort -uV | tail -1 || true)
    echo "highest glibc requirement: ${MAX:-none}"
    [ -n "$MAX" ] || fail "no GLIBC_ references at all -- statically linked glibc? \
that is not portable: NSS and gconv load modules with dlopen at run time and \
then need the glibc it was linked against"
    HAVE=${MAX#GLIBC_2.}
    WANT=${FLOOR#2.}
    [ "$HAVE" -le "$WANT" ] || fail "requires $MAX, allowed at most GLIBC_$FLOOR"

    if has --armv6-hardfloat; then
      echo "--- ARM ABI ---"
      readelf -A "$BIN" | grep -E 'Tag_CPU_arch|Tag_FP_arch|Tag_ABI_VFP_args' || true
      readelf -A "$BIN" | grep -q 'Tag_CPU_arch: v6' \
        || fail "not armv6 -- this drops the Pi 1 and Zero"
      readelf -A "$BIN" | grep -q 'Tag_ABI_VFP_args: VFP registers' \
        || fail "not hard-float -- this does not run on Raspberry Pi OS"
    fi
    ;;

  *Mach-O*)
    echo "--- libraries it needs at start ---"
    otool -L "$BIN"
    echo "--- deployment target ---"
    otool -l "$BIN" | grep -A4 LC_BUILD_VERSION || true
    MINOS=$(vtool -show-build "$BIN" 2>/dev/null | awk '/minos/ {print $2; exit}' || true)
    echo "minos: ${MINOS:-unknown}"
    [ -n "$MINOS" ] || fail "could not read a deployment target from $BIN"
    case "$MINOS" in
      "$FLOOR"|"$FLOOR".*) ;;
      *) fail "asked for macOS $FLOOR, binary says $MINOS -- is \
MACOSX_DEPLOYMENT_TARGET set for the whole build, not just for gdb?" ;;
    esac
    ;;

  *PE*|*MS\ Windows*)
    OD=objdump
    command -v x86_64-w64-mingw32-objdump >/dev/null && OD=x86_64-w64-mingw32-objdump

    echo "--- subsystem version in the header (informational only) ---"
    "$OD" -p "$BIN" | grep -iE 'Major(OSystem|Subsystem)Version|Minor(OSystem|Subsystem)Version' \
      || echo "(could not read the PE header with $OD)"
    # This number is not the floor, however much it looks like one. It is a
    # default the linker stamps, and the default differs per target: our two
    # clients are built from the same sources with the same _WIN32_WINNT, and
    # the 32-bit one says 4.0 while the 64-bit one says 5.2 -- which is simply
    # the oldest Windows that ever had the respective architecture. The loader
    # only refuses a binary whose number is HIGHER than the running Windows, so
    # a low stamp can never make a program portable; it just fails to say
    # anything. Comparing it against a floor would therefore pass every time.
    #
    # What does decide the floor is the import table: a function that Windows 7
    # introduced makes the client refuse to start on Vista, no matter what any
    # header field claims. So that is what we look at.

    echo "--- DLLs it needs at start ---"
    "$OD" -p "$BIN" | awk '/DLL Name:/ {print "  " $3}' | sort -u

    # Two DLL names are floors in themselves:
    #   api-ms-win-*     the UCRT -- Windows 10, or a redistributable before it
    #   kernelbase.dll   Windows 7
    # mingw-w64 normally links the old msvcrt.dll, which every Windows has.
    DLLS=$("$OD" -p "$BIN" | awk '/DLL Name:/ {print tolower($3)}' | sort -u)
    case "$DLLS" in
      *api-ms-win-crt*)
        has --ucrt-ok || fail "imports the UCRT (api-ms-win-crt-*) -- that is a \
Windows 10 floor unless the user installs a redistributable"
        echo "(imports the UCRT, as a MSYS2 UCRT64 build does: Windows 10)" ;;
    esac
    case "$DLLS" in
      *kernelbase.dll*) fail "imports kernelbase.dll -- that DLL arrived with \
Windows 7, so the client cannot start on Vista" ;;
    esac

    # And a look at the other end: do we actually use anything newer than XP?
    # Purely informational -- finding nothing would not prove the client runs on
    # XP, only that these particular names are absent. But finding something
    # confirms that _WIN32_WINNT=0x0600 is a real requirement and not a habit.
    echo "--- functions that are Vista or newer (informational) ---"
    "$OD" -p "$BIN" \
      | grep -oE '\b(GetTickCount64|InitializeConditionVariable|SleepConditionVariableCS|InitializeSRWLock|AcquireSRWLockExclusive|CreateSymbolicLinkW|GetFinalPathNameByHandleW|InetNtopW|CancelIoEx|GetThreadId)\b' \
      | sort -u | sed 's/^/  /' || echo "  (none of the names we look for)"

    echo "OK: nothing in the import table of $BIN demands more than Windows $FLOOR"
    exit 0
    ;;

  *)
    fail "do not know how to read $BIN: $KIND"
    ;;
esac

echo "OK: $BIN stays within $FLOOR"
