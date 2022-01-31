#ifndef TFS_SERVER
#define TFS_SERVER

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>


#define S 10
#define NAME_SIZE 40

/*
 * Buffer entry
 */
typedef struct {
    char opcode;
    char name[NAME_SIZE];
    int fhandle;
    int flags;
    size_t len;
    char *buffer;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} buffer_entry;

int addClientPipe(char *client_pipe_path);
void *requestHandler(void* arg);


#endif // TFS_SERVER