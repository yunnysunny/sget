#!/bin/sh
set -eu

# Build sget. On Windows builds (MinGW/MSYS2) pass CC plus -DWIN32 -lws2_32;
# the defaults below target POSIX.
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -Wall -Wextra -O2}"
SOURCES="log.c win2linux.c common_socket.c sget_thread.c metadata.c download.c utest.c"

case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        # shellcheck disable=SC2086
        exec "$CC" -DWIN32 -D_CRT_SECURE_NO_WARNINGS $CFLAGS \
            -o sget.exe $SOURCES -lws2_32
        ;;
    *)
        # shellcheck disable=SC2086
        exec "$CC" -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 $CFLAGS \
            -pthread -o sget $SOURCES
        ;;
esac
