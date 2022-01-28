#include "tecnicofs_client_api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int fserver, fclient;
static const char *client_path;
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    void *buffer = malloc(TFS_MOUNT_SIZE);
    size_t buffer_size = 0;

    // Create pipe
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path, strerror(errno));
        return -1;
    }
    if (mkfifo(client_pipe_path, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return -1;    }
    if ((fserver = open (server_pipe_path, O_WRONLY)) == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
    }

    client_path = client_pipe_path;
    int opcode = 1;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, client_pipe_path, TFS_PIPENAME_SIZE);
    buffer_size += sizeof(char[40]);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if ((fclient = open (client_pipe_path, O_RDONLY)) == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
    }

    if (read(fclient, &session_id, sizeof(int)) == -1)
        return -1;

    if(session_id == -1)
        return -1;

    return 0;
}


int tfs_unmount() {
    void *buffer = malloc(TFS_UNMOUNT_SIZE);
    size_t buffer_size = 0;

    int opcode = 2;
    int return_value;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value != 0)
        return -1;
    
    session_id = -1;
    // Close pipe
    close (fserver);
    close (fclient);
    if (unlink(client_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_path, strerror(errno));
        return -1;
    }

    return 0;
}


int tfs_open(char const *name, int flags) {
    void *buffer = malloc(TFS_OPEN_SIZE);
    size_t buffer_size = 0;

    int opcode = 3;
    int fhandle;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, name, TFS_NAME_SIZE);
    buffer_size += sizeof(char[40]);
    memcpy(buffer + buffer_size, &flags, TFS_FLAGS_SIZE);
    buffer_size += sizeof(int);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &fhandle, sizeof(int)) == -1)
        return -1;

    return fhandle;
}


int tfs_close(int fhandle) {
    void *buffer = malloc(TFS_CLOSE_SIZE);
    size_t buffer_size = 0;

    int opcode = 4;
    int return_value;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &fhandle, TFS_FHANDLE_SIZE);
    buffer_size += sizeof(int);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    return return_value;
}


ssize_t tfs_write(int fhandle, void const *write_buffer, size_t len) {
    void *buffer = malloc(TFS_WRITE_SIZE + sizeof(char[len]));
    size_t buffer_size = 0;

    int opcode = 5;
    ssize_t write_size;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &fhandle, TFS_FHANDLE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &len, TFS_LEN_SIZE);
    buffer_size += sizeof(size_t);
    memcpy(buffer + buffer_size, write_buffer, sizeof(char[len]));
    buffer_size += sizeof(char[len]);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
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
    void *buffer = malloc(TFS_READ_SIZE);
    size_t buffer_size = 0;

    int opcode = 6;
    ssize_t read_size;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &fhandle, TFS_FHANDLE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &len, TFS_LEN_SIZE);
    buffer_size += sizeof(size_t);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
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
    void *buffer = malloc(TFS_SHUTDOWN_SIZE);
    size_t buffer_size = 0;

    int opcode = 7;
    int return_value;

    // Create buffer
    memcpy(buffer + buffer_size, &opcode, TFS_OPCODE_SIZE);
    buffer_size += sizeof(int);
    memcpy(buffer + buffer_size, &session_id, TFS_SESSIONID_SIZE);
    buffer_size += sizeof(int);

    // Write and read the pipe
    if (write_on_pipe(buffer, buffer_size) == -1)
        return -1;
    free(buffer);
    if (read(fclient, &return_value, sizeof(int)) == -1)
        return -1;

    if (return_value == 0){
        return 0;
    }

    return -1;
}


int write_on_pipe(void *buffer, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t ret = write(fserver, buffer + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            return -1;
        }
        written += (size_t)ret;
    }
    return 0;
}