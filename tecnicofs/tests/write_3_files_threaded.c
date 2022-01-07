#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 40
#define SIZE 3000
#define N 4

/**
   This test fills in a new file up to 10 blocks via multiple writes, 
   each write always targeting only 1 block of the file, 
   then checks if the file contents are as expected
 */


struct arguments{
    char path[N];
    char input[SIZE];
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
    struct arguments *args = (struct arguments*) arg;
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

    struct arguments args1;
    char path1[N] = "/f1";
    memcpy(args1.path, &path1, N);
    memset(args1.input, 'A', SIZE);

    struct arguments args2;
    char path2[N] = "/f2";
    memcpy(args2.path, &path2, N);
    memset(args2.input, 'B', SIZE);

    struct arguments args3;
    char path3[N] = "/f3";
    memcpy(args3.path, &path3, N);
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
    
    if (pthread_create(&tid[0], NULL, fnRead, (void*)&args1) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, fnRead, (void*)&args2) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[2], NULL, fnRead, (void*)&args3) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);

    assert(memcmp(args1.input, args1.output, SIZE) == 0);
    assert(memcmp(args2.input, args2.output, SIZE) == 0);
    assert(memcmp(args3.input, args3.output, SIZE) == 0);

    printf("Sucessful test\n");

    return 0;
}