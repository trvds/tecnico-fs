#ifndef TFS_SERVER
#define TFS_SERVER

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>


#define S 20
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

void write_on_pipe(int fclient, void *buffer, size_t len);
void read_from_pipe(int fserver, void *buffer, size_t len);
int addClientPipe(char *client_pipe_path);
void removeClientPipe(int session_id);
void *requestHandler(void* arg);

#endif // TFS_SERVER