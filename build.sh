#!/bin/sh
set -eu

# Build sget. On Windows builds (MinGW/MSYS2) pass CC plus -DWIN32 -lws2_32;
# the defaults below target POSIX.
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -Wall -Wextra -O2}"
# Override to stamp a release tag into the binary, e.g. SGET_VERSION=v1.0.0.
# The value is single-quoted so the shell strips the quotes and the compiler
# receives a proper string literal: -DSGET_VERSION="v1.0.0".
VERSION="${SGET_VERSION:-0.0.0-dev}"
SOURCES="log.c win2linux.c common_socket.c sget_thread.c metadata.c download.c utest.c"

# Detect the Windows toolchain. MSYS2/MinGW report MINGW*/MSYS*/CYGWIN*, but
# w64devkit's uname prints Windows_NT, so fall back to the MINGW_PREFIX that
# MSYS2 and w64devkit both export.
case "${MINGW_PREFIX:-}$(uname -s 2>/dev/null || echo unknown)" in
    *MINGW*|*MSYS*|*CYGWIN*|*Windows*)
        # shellcheck disable=SC2086
        exec "$CC" -DWIN32 -D_CRT_SECURE_NO_WARNINGS -DSGET_VERSION='"'"$VERSION"'"' $CFLAGS \
            -o sget.exe $SOURCES -lws2_32
        ;;
    *)
        # shellcheck disable=SC2086
        exec "$CC" -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 -DSGET_VERSION='"'"$VERSION"'"' $CFLAGS \
            -pthread -o sget $SOURCES
        ;;
esac
