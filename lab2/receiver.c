#include "rtp.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

int listen_port;
char file_path[256];
int window_size;

void conn() {
    
}

int main(int argc, char **argv) {
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }

    listen_port = atoi(argv[1]);
    strncpy(file_path, argv[2], 256);
    window_size = atoi(argv[3]);

    conn();

    LOG_DEBUG("Receiver: exiting...\n");
    return 0;
}
