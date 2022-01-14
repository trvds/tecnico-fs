#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define NUMBER_OF_THREADS 100
#define SIZE 12
#define N 4
/**
   This test checks if the creation, writing and reading is working
   with multiple threads.
 */


struct arguments{
    char path[N];
    char input[SIZE];
};

void *fnWrite(void *arg){
    /* Buffers for the path and input */
    struct arguments *args = (struct arguments*) arg;
    
    char path[SIZE];
    memcpy(path, args->path, N);
    char input[SIZE];
    memcpy(input, args->input, SIZE);
    /* Open File to append */
    int fd = tfs_open(path, TFS_O_APPEND);
    assert(fd != -1);

    assert(tfs_write(fd, input, SIZE) == SIZE);
    assert(tfs_close(fd) != -1);
    return NULL;
}

int main() {
    pthread_t tid[NUMBER_OF_THREADS];
    struct arguments args;

    char *input = "Lorem Ipsum";
    memcpy(args.input, input, SIZE);
    char *path = "/f1";
    memcpy(args.path, path, N);

    assert(tfs_init() != -1);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);
    assert(tfs_close(f) != -1);

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnWrite, (void*)&args) != 0)
            exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        pthread_join(tid[i], NULL);
    }

    char output[SIZE];

    int fd = tfs_open(path, 0);
    assert(fd != -1);

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert(strcmp(output, input) == 0);
    }

    printf("Sucessful test\n");

    return 0;
}