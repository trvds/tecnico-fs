#include "fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 40
#define SIZE 3000
#define N 4

/**
   Thit test will test 3 thread writing in the same file and check if there is corruption in the file.
 */


struct arguments{
    char path[N];
    char input[SIZE];
};

struct output{
    char path[N];
    char output[SIZE];
};

void *fnWrite(void *arg){
    struct arguments *args = (struct arguments*) arg;
    char path[N];
    char input[SIZE];
    memcpy(path, args->path, N);
    memcpy(input, args->input, SIZE);
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    assert(tfs_write(fd, input, SIZE) == SIZE);
    assert(tfs_close(fd) != -1);
    return NULL;
}

void *fnRead(void *arg){
    struct output *args = (struct output*) arg;
    char path[N];
    char output[SIZE];
    memcpy(path, args->path, N);
    int fd = tfs_open(path, 0);
    assert(fd != -1 );

    assert(tfs_read(fd, output, SIZE) == SIZE);
    memcpy(args->output, output, SIZE);

    assert(tfs_close(fd) != -1);
    return NULL;
}

int main() {
    pthread_t tid[3];

    char path[N] = "/f1";

    struct arguments args1;
    memcpy(args1.path, &path, N);
    memset(args1.input, 'A', SIZE);

    struct arguments args2;
    memcpy(args2.path, &path, N);
    memset(args2.input, 'B', SIZE);

    struct arguments args3;
    memcpy(args3.path, &path, N);
    memset(args3.input, 'C', SIZE);

    assert(tfs_init() != -1);

    if (pthread_create(&tid[0], NULL, fnWrite, (void*)&args1) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, fnWrite, (void*)&args2) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[2], NULL, fnWrite, (void*)&args3) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    
    struct output output;
    memcpy(output.path, &path, N);

    fnRead((void*)&output);
    if(!(memcmp(args1.input, output.output, SIZE) == 0 || memcmp(args2.input, output.output, SIZE) == 0 || memcmp(args3.input, output.output, SIZE) == 0)){
        printf("error");
    }

    printf("Sucessful test\n");

    return 0;
}