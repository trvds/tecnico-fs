#include "operations.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define S 1
#define BUFFER_SIZE 1000




void requestHandler(char buffer[]){
    char * opcode = strtok(buffer, "|");

    if (strcmp(opcode, "OP_CODE=1") == 0){
        char client_pipe_name[40] = strtok(NULL, "|");

    }
    else if (strcmp(opcode, "OP_CODE=2") == 0){
        int session_id = strtok(NULL, "|") - "0";

    }
    else if (strcmp(opcode, "OP_CODE=3") == 0){
        int session_id = strtok(NULL, "|") - "0";
        char name[40] = strtok(NULL, "|");
        int flags = strtok(NULL, "|") - "0";

    }
    else if (strcmp(opcode, "OP_CODE=4") == 0){
        int session_id = strtok(NULL, "|") - "0";
        int fhandle = strtok(NULL, "|") - "0";

    }
    else if (strcmp(opcode, "OP_CODE=5") == 0){
        int session_id = strtok(NULL, "|") - "0";
        int fhandle = strtok(NULL, "|") - "0";
        size_t len = (size_t) (strtok(NULL, "|") - "0");
        char buffer_content[len] = strtok(NULL, "|");
    }
    else if (strcmp(opcode, "OP_CODE=6") == 0){
        int session_id = strtok(NULL, "|") - "0";
        int fhandle = strtok(NULL, "|") - "0";
        size_t len = (size_t) (strtok(NULL, "|") - "0");

    }
    else if (strcmp(opcode, "OP_CODE=7") == 0){
        int session_id = strtok(NULL, "|") - "0";

    }

    
}



int main(int argc, char **argv) {
    pthread_t tid[S];
    int thread_counter = 0;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    int fserver, buffer_reader;
    char buffer[BUFFER_SIZE];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    unlink(pipename);
    
    if (mkfifo (pipename, 0777) < 0) exit (EXIT_FAILURE);

    if ((fserver = open (pipename, O_RDONLY)) < 0) exit(EXIT_FAILURE);

    while(1){
        buffer_reader = read(fserver, buffer, BUFFER_SIZE);
        requestHandler(buffer);

    }

    close (fserver);
    unlink(pipename);

    return 0;
}