#!/bin/bash
gcc -o sget log.c win2linux.c common_socket.c sget_thread.c metadata.c download.c utest.c -lpthread
