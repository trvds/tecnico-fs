#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_NULL = 0,
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

/* data size (for client-server requests) */
enum {
    TFS_OPCODE_SIZE = sizeof(char),
    TFS_PIPENAME_SIZE = sizeof(char[40]),
    TFS_SESSIONID_SIZE = sizeof(int),
    TFS_NAME_SIZE = sizeof(char[40]),
    TFS_FLAGS_SIZE = sizeof(int),
    TFS_FHANDLE_SIZE = sizeof(int),
    TFS_LEN_SIZE = sizeof(size_t)
};

/* return requests size */
enum {
    TFS_MOUNT_RETURN_SIZE = TFS_SESSIONID_SIZE,
    TFS_UNMOUNT_RETURN_SIZE = sizeof(int),
    TFS_OPEN_RETURN_SIZE = sizeof(int),
    TFS_CLOSE_RETURN_SIZE = sizeof(int),
    TFS_WRITE_RETURN_SIZE = sizeof(ssize_t),
    TFS_READ_RETURN_SIZE = sizeof(ssize_t),
    TFS_SHUTDOWN_RETURN_SIZE = sizeof(int)
};

#endif /* COMMON_H */