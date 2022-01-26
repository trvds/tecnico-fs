#include "tecnicofs_client_api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static int fserver, fclient;
static const char *client_path;
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    void *buffer = malloc(sizeof(int) + sizeof(char[40]));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    // Create pipe
    unlink(client_pipe_path);
    if (mkfifo (client_pipe_path, 0777) < 0)
        return -1;
    if ((fclient = open (client_pipe_path, O_RDONLY)) < 0)
        return -1;
    if ((fserver = open (server_pipe_path, O_WRONLY)) < 0)
        return -1;

    client_path = client_pipe_path;
    int opcode = 1;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, client_pipe_path, sizeof(char[40]));
    buffer_size += sizeof(char[40]);
    buffer_offset += sizeof(char[40]);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;   
    free(buffer);
    if (read(fclient, &session_id, sizeof(int)) == -1)
        return -1;

    if(session_id == -1)
        return -1;

    return 0;
}


int tfs_unmount() {
    void *buffer = malloc(sizeof(int) + sizeof(int));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 2;
    int return_value;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value != 0)
        return -1;
    
    session_id = -1;
    // Close pipe
    close (fserver);
    close (fclient);
    unlink(client_path);

    return 0;
}


int tfs_open(char const *name, int flags) {
    void *buffer = malloc(sizeof(int) + sizeof(int) + sizeof(char[40]) + sizeof(int));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 3;
    int fhandle;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, name, sizeof(char[40]));
    buffer_size += sizeof(char[40]);
    buffer_offset += sizeof(char[40]);
    memcpy(buffer_offset, &flags, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &fhandle, sizeof(int)) == -1)
        return -1;

    if (fhandle > 0){
        return fhandle;
    }

    return -1;
}


int tfs_close(int fhandle) {
    void *buffer = malloc(sizeof(int) + sizeof(int) + sizeof(int));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 4;
    int return_value;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &fhandle, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value == 0){
        return 0;
    }

    return -1;
}


ssize_t tfs_write(int fhandle, void const *write_buffer, size_t len) {
    void *buffer = malloc(sizeof(int) + sizeof(int) + sizeof(int) + sizeof(size_t) + sizeof(char[len]));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 5;
    ssize_t write_size;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &fhandle, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &len, sizeof(size_t));
    buffer_size += sizeof(size_t);
    buffer_offset += sizeof(size_t);
    memcpy(buffer_offset, write_buffer, sizeof(char[len]));
    buffer_size += sizeof(char[len]);
    buffer_offset += sizeof(char[len]);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &write_size, sizeof(ssize_t)) == -1)
        return -1;

    if (write_size > 0){
        return write_size;
    }

    return -1;
}


ssize_t tfs_read(int fhandle, void *read_buffer, size_t len) {
    void *buffer = malloc(sizeof(int) + sizeof(int) + sizeof(int) + sizeof(size_t));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 6;
    ssize_t read_size;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &fhandle, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &len, sizeof(size_t));
    buffer_size += sizeof(size_t);
    buffer_offset += sizeof(size_t);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &read_size, sizeof(ssize_t)) == -1)
        return -1;
    if (read(fclient, read_buffer, sizeof(char[read_size])) == -1)
        return -1;

    if (read_size > 0){
        return read_size;
    }

    return -1;
}


int tfs_shutdown_after_all_closed() {
    void *buffer = malloc(sizeof(int) + sizeof(int));
    void *buffer_offset = buffer;
    size_t buffer_size = 0;

    int opcode = 7;
    int return_value;

    // Create buffer
    memcpy(buffer_offset, &opcode, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);
    memcpy(buffer_offset, &session_id, sizeof(int));
    buffer_size += sizeof(int);
    buffer_offset += sizeof(int);

    // Write and read the pipe
    if (write(fserver, buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value == 0){
        return 0;
    }

    return -1;
}
