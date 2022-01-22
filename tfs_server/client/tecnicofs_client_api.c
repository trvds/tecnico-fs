#include "tecnicofs_client_api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 1000

static int fserver, fclient;
static char buffer[BUFFER_SIZE];
static char* client_path;
static char* server_path;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    client_path = client_pipe_path;
    server_path = server_pipe_path;
    unlink(client_pipe_path);
    if (mkfifo (client_pipe_path, 0777) < 0) return -1;

    if ((fserver = open (client_pipe_path, O_WRONLY)) < 0) return -1;
    if ((fclient = open (server_pipe_path, O_RDONLY)) < 0) return -1;
    return 0;
}

int tfs_unmount() {
    strcpy(buffer, "OP_CODE=2 | ")
    unlink(client_path);
    close (fserver);
    close (fclient);
    return 0;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
