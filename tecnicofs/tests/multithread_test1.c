#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define SIZE 45056 // 256 x 4 x 11
#define N 5
#define NUMBER_OF_THREADS 20 // Não pode ser mais que 20 pela limitação do file system de ter 20 ficheiros abertos consecutivamente

/**
   This test checks if the creation, writing and reading is working
   with multiple threads.
 */


struct arguments{
    char path[N];
    char input[SIZE];
    char output[SIZE];
};

void *fnWrite(void *arg){
    /* Buffers for the path and input */
    struct arguments *args = (struct arguments*) arg;
    char path[N];
    char input[SIZE];
    memcpy(path, args->path, N);
    memcpy(input, args->input, SIZE);
    /* Create file*/
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    assert(tfs_write(fd, input, SIZE) == SIZE);
    assert(tfs_close(fd) != -1);
    return NULL;
}

void *fnRead(void *arg){
    /* Buffers for the path and output */
    struct arguments *args = (struct arguments*) arg;
    char path[N];
    char output[SIZE];
    memcpy(path, args->path, N);
    /* Open file */
    int fd = tfs_open(path, 0);
    assert(fd != -1 );

    assert(tfs_read(fd, output, SIZE) == SIZE);
    memcpy(args->output, output, SIZE);

    assert(tfs_close(fd) != -1);
    return NULL;
}

int main() {
    pthread_t tid[NUMBER_OF_THREADS];
    struct arguments args[NUMBER_OF_THREADS];
    char letter = 'A';
    /* Declaring the arguments in the argument structure */
    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        char path[N];
        sprintf(path, "/f%d", i);
        memcpy(args[i].path, path, N);
        memset(args[i].input, letter, SIZE);
        letter++;
    }

    assert(tfs_init() != -1);

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnWrite, (void*)&args[i]) != 0)
            exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        pthread_join(tid[i], NULL);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnRead, (void*)&args[i]) != 0)
            exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        pthread_join(tid[i], NULL);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        assert(memcmp(args[i].input, args[i].output, SIZE) == 0);
    }

    printf("Sucessful test\n");

    return 0;
}