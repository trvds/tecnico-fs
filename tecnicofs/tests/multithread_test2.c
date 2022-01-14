#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define NUMBER_OF_THREADS 20 // Não pode ser mais que 20 pela limitação do file system de ter 20 ficheiros abertos consecutivamente

/**
   This test checks if the creation, writing and reading is working
   with multiple threads.
 */


struct arguments{
    char *path;
    char *input;
};

void *fnWrite(void *arg){
    /* Buffers for the path and input */
    struct arguments *args = (struct arguments*) arg;
    char *path;
    char *input;
    memcpy(path, args->path, strlen(path));
    memcpy(input, args->input, strlen(input));
    /* Create file*/
    int fd = tfs_open(path, TFS_O_APPEND);
    assert(fd != -1);

    assert(tfs_write(fd, input, strlen(input)) == strlen(input));
    assert(tfs_close(fd) != -1);
    return NULL;
}

int main() {
    pthread_t tid[NUMBER_OF_THREADS];
    struct arguments args;
    char *input = 'Lorem Ipsum';
    /* Declaring the arguments in the argument structure */
    char *path = "/f1";
    char output[strlen(input)];
    memcpy(args.path, path, strlen(path));
    memset(args.input, input, strlen(input));

    assert(tfs_init() != -1);

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnWrite, (void*)&args) != 0)
            exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        pthread_join(tid[i], NULL);
    }
    
    int fd = tfs_open(path, 0);
    assert(fd != -1);

    for (int i = 0; i < NUMBER_OF_THREADS; i++){
        assert(tfs_read(fd, output, strlen(output) - 1) == strlen(input));
        output[strlen(input)] == '\0';
        assert(strcmp(output, args.input) == 0);
    }

    printf("Sucessful test\n");

    return 0;
}