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
static const char *client_path;
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    unlink(client_pipe_path);
    if (mkfifo (client_pipe_path, 0777) < 0)
        return -1;

    if ((fclient = open (client_pipe_path, O_RDONLY)) < 0)
        return -1;
    if ((fserver = open (server_pipe_path, O_WRONLY)) < 0)
        return -1;
    client_path = client_pipe_path;

    int opcode = 1;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, client_pipe_path, sizeof(char[40])) == -1)
        return -1;
    if (read(fclient, &session_id, sizeof(int)) == -1)
        return -1;

    if(session_id == -1)
        return -1;

    return 0;
}

int tfs_unmount() {
    int opcode = 2;
    int return_value;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value != 0)
        return -1;
    
    session_id = -1;
    
    close (fserver);
    close (fclient);
    unlink(client_path);
    return 0;
}

int tfs_open(char const *name, int flags) {
    int opcode = 3;
    int fhandle;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (write(fserver, name, sizeof(char[40])) == -1)
        return -1;
    if (write(fserver, &flags, sizeof(int)) == -1)
        return -1;
    if (read(fclient, &fhandle, sizeof(int)) == -1)
        return -1;

    if (fhandle > 0){
        return fhandle;
    }

    return -1;
}

int tfs_close(int fhandle) {
    int opcode = 4;
    int return_value;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &fhandle, sizeof(int)) == -1)
        return -1;
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value == 0){
        return 0;
    }

    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    int opcode = 5;
    ssize_t write_size;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &fhandle, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &len, sizeof(size_t)) == -1)
        return -1;
    if (write(fserver, buffer, sizeof(char[len])) == -1)
        return -1;
    if (read(fclient, &write_size, sizeof(ssize_t)) == -1)
        return -1;

    if (write_size > 0){
        return write_size;
    }

    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    int opcode = 6;
    ssize_t read_size;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &fhandle, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &len, sizeof(size_t)) == -1)
        return -1;
    if (read(fclient, &read_size, sizeof(ssize_t)) == -1)
        return -1;
    if (read(fclient, buffer, sizeof(char[read_size])) == -1)
        return -1;

    if (read_size > 0){
        return read_size;
    }

    return -1;
}

int tfs_shutdown_after_all_closed() {
    int opcode = 7;
    int return_value;

    if (write(fserver, &opcode, sizeof(int)) == -1)
        return -1;
    if (write(fserver, &session_id, sizeof(int)) == -1)
        return -1;
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value == 0){
        return 0;
    }

    return -1;
}
