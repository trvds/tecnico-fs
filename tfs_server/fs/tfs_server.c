#include "operations.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define S 1

void requestHandler(int opcode, int session_id){
    switch (opcode){
        case 1:
            // TO DO
        break;
        case 2:
            // TO DO
        break;
        case 3:
            // TO DO
        break;
        case 4:
            // TO DO
        break;
        case 5:
            // TO DO
        break;
        case 6:
            // TO DO        
        break;
        case 7:
            // TO DO
        break;
        default:
            // TO DO
    }
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    static char *pipename;
    memcpy(pipename, argv[1], strlen(argv[1]));
    static int fserver;
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    unlink(pipename);
    if (mkfifo (pipename, 0777) < 0) exit (EXIT_FAILURE);

    if ((fserver = open (pipename, O_RDONLY)) < 0) exit(EXIT_FAILURE);

    while(1){
        // TO DO
    }

    close (fserver);
    unlink(pipename);

    return 0;
}