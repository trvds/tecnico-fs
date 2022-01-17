#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define NUMBER_OF_THREADS 20
#define UPPER 20
#define LOWER 1

char *randomstring(size_t len) {
    static char chars[] = "abcdefghijklmnopqrstuvwxyz";        
    char *randomString = NULL;
    if(len){
        randomString = malloc(sizeof(char)*(len+1));
        if(randomString){            
            for(int i = 0 ; i < len ; i++){            
                int ss = rand()%(int)(sizeof(chars)-1);
                randomString[i] = chars[ss];
            }
            randomString[len] = '\0';
        }
    }
    return randomString;
}


void *fnOpen(void *arg){
    (void)arg;
    
    size_t length = (size_t)((rand() % (UPPER - LOWER + 1)) + LOWER);
    char *input = randomstring(length);
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