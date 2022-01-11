#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define NUMBER_OF_THREADS 20
#define UPPER 20
#define LOWER 1

/* Função retirada da internet */
char *randstring(size_t length) {
    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";        
    char *randomString = NULL;

    if (length) {
        randomString = malloc(sizeof(char) * (length +1));
        if (randomString) {            
            for (int n = 0;n < length;n++) {            
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }

            randomString[length] = '\0';
        }
    }
    return randomString;
}


void *fnOpen(void *arg){
    (void)arg;
    
    size_t length = (size_t)((rand() % (UPPER - LOWER + 1)) + LOWER);
    char *input = randstring(length);
    char *path = malloc(sizeof(char)*(length + 1));
    strcat(path, "/");
    strcat(path, input);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_close(fd) != -1);
    
    free(path);
    return NULL;
}


int main() {
    srand((unsigned int)time(NULL));   
    pthread_t tid[NUMBER_OF_THREADS];
    assert(tfs_init() != -1);

    for(int i = 0; i < NUMBER_OF_THREADS; i++){
        if (pthread_create(&tid[i], NULL, fnOpen, NULL) != 0)
            exit(EXIT_FAILURE);
    }

    for(int i = 0; i < NUMBER_OF_THREADS; i++){
       if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }

    printf("Successful test.\n");

    return 0;
}