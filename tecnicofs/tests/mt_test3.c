#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define NUMBER_OF_THREADS 1000
#define SIZE 1000


void *fnRead(void* arg){
    (void)arg;
    char *path = "/f1";
    char input[SIZE];
    char output[SIZE];
    memset(input, 'A', SIZE);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    
    assert(tfs_read(fd, output, SIZE) == SIZE);
    
    assert(tfs_close(fd) != -1);
    
    assert(memcmp(input, output, SIZE) == 0);
    return NULL;
}


int main() {
    pthread_t tid[NUMBER_OF_THREADS];
    assert(tfs_init() != -1);
    char input[SIZE];

    memset(input, 'A', SIZE);

    char *path = "/f1";
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, input, SIZE) == SIZE);
    assert(tfs_close(fd) != -1);


    for(int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnRead, NULL) != 0)
            exit(EXIT_FAILURE);
    }

    for(int i = 0; i < NUMBER_OF_THREADS; i++){
       if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }

    printf("Successful test.\n");

    return 0;
}