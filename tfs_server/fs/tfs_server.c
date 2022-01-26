#include "operations.h"
#include "tfs_server.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


// Buffers
buffer_entry buffer_entry_table[S];
// Client Pipe Paths table;
char *client_pipes_table[S];


void *requestHandler(void* arg){
    // Get arguments
    int session_id = *((int *) arg);
    free(arg);
    printf("Request Handler no %d initialized\n", session_id);
    buffer_entry *buffer = &buffer_entry_table[session_id];

    // Initialize mutex and conditional variable
    pthread_mutex_init(&buffer->lock, NULL);
    pthread_cond_init(&buffer->cond, NULL);

    // Open client pipe
    int fclient = -1;

    // Start receiving requests;
    int receiving_requests = 1;
    while(receiving_requests){
        int return_value;
        ssize_t return_len;
        char *read_buffer = "";

        // Lock buffer
        if (pthread_mutex_lock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);

        // Wait for signal that we can read the buffer
        while (!(buffer->opcode > 0)){
            pthread_cond_wait(&buffer->cond, &buffer->lock);
        }
        
        // Read buffer
        switch (buffer->opcode){
            // tfs_mount
            case 1:
                printf("Request Handler no %d: mounting\n", session_id);       
                if ((fclient = open(client_pipes_table[session_id], O_WRONLY)) < 0)
                    exit(EXIT_FAILURE);
                printf("client pipe opened on request handler no %d\n", session_id);       
                if (write(fclient, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("written the return by request handler no %d\n", session_id);       

            // tfs_unmount
            break;
            case 2:
                printf("Request Handler no %d: unmounting\n", session_id);       
                return_value = 0;
                client_pipes_table[session_id] = NULL;
                if (write(fclient, &return_value, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                close(fclient);
            break;
            // tfs_open
            case 3:
                printf("Request Handler no %d: opening\n", session_id);       
                return_value = tfs_open(buffer->name, buffer->flags);
                if (write(fclient, &return_value, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("written the return value %d by request handler no %d\n", return_value, session_id);       
            break;
            //tfs_close
            case 4:
                printf("Request Handler no %d: closing\n", session_id);       
                return_value = tfs_close(buffer->fhandle);
                if (write(fclient, &return_value, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("written the return by request handler no %d\n", session_id);       
            break;
            // tfs_write
            case 5:
                printf("Request Handler no %d: writing\n", session_id);       
                return_len = tfs_write(buffer->fhandle, buffer->buffer, buffer->len);
                free(buffer->buffer);
                if (write(fclient, &return_len, sizeof(ssize_t)) == -1)
                    exit(EXIT_FAILURE);
                printf("written the return len by request handler no %d\n", session_id);       
            break;
            // tfs_read
            case 6:
                printf("Request Handler no %d: reading\n", session_id);       
                read_buffer = malloc(sizeof(char));
                return_len = tfs_read(buffer->fhandle, read_buffer, buffer->len);
                if (write(fclient, &return_len, sizeof(ssize_t)) == -1)
                    exit(EXIT_FAILURE);
                if (write(fclient, read_buffer, sizeof(char[buffer->len])) == -1)
                    exit(EXIT_FAILURE);
                free(read_buffer);
            break;
            // tfs_shutdown_after_all_closed
            case 7:
                printf("Request Handler no %d: shutting down\n", session_id);       
                return_value = tfs_destroy_after_all_closed();
                if (write(fclient, &return_value, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
            break;
            // Error checking
            default:
            break;
        }
        buffer->opcode = 0;

        // Unlock buffer
        if (pthread_mutex_unlock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);
    }
    pthread_exit(NULL);
}


int addClientPipe(char *client_pipe_path){
    printf("Adding Client to client_pipe_table\n");
    int session_id;
    //Check entry with same client path
    printf("Checking if there is a pipe with the same name\n");
    for(int i = 0; i < S; i++){
        if (client_pipes_table[i] != NULL && strcmp(client_pipes_table[i], client_pipe_path) == 0){
            session_id = i;
            printf("Found pipe with same path\n");
            return 0;
        }
    }
    // Check first free entry on pipes table
    printf("Checking if there is a free slot on table\n");
    for(int i = 0; i < S; i++){
        if(client_pipes_table[i] == NULL){
            // Session id will be the client pipe index on pipe table
            session_id = i;
            printf("New session_id: %d\n", session_id);
            // Store pipe path on pipe table
            client_pipes_table[i] = client_pipe_path;
            printf("Stored pipe path on server\n");
            return 0;
        }
    }
    return -1;
}



int main(int argc, char **argv) {
    // Worker threads
    pthread_t worker_threads_table[S];

    // Initialize client_pipe_table
    for(int i = 0; i < S; i++){
        client_pipes_table[i] = NULL;
    }

    //Argument Check
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    // Starting tfs_server
    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    
    // Start file system
    if (tfs_init() == -1)
        exit(EXIT_FAILURE);

    // Create worker threads
    for (int i = 0; i < S; i++){
        int *session_id = malloc(sizeof(*session_id));
        *session_id = i;
        if(pthread_create(&worker_threads_table[i], NULL, requestHandler, (void*)session_id) == -1)
            exit(EXIT_FAILURE);
    }
    printf("Created worker threads\n");
    fflush(stdout);

    // Create server pipe
    unlink(pipename);
    if (mkfifo (pipename, 0777) < 0)
        exit (EXIT_FAILURE);
    int fserver;
    if ((fserver = open (pipename, O_RDONLY)) < 0)
        exit(EXIT_FAILURE);
    printf("Created server pipe\n");
    fflush(stdout);

    // Wait for arguments
    while(1){
        int opcode;
        int session_id;
        char client_pipe_path[CLIENT_PIPE_PATH_SIZE];
        buffer_entry *buffer;
        // Get request opcode from pipe
        if ((fserver = open (pipename, O_RDONLY)) < 0)
            exit(EXIT_FAILURE);
        printf("Main thread: reading pipe\n");
        fflush(stdout);
        if (read(fserver, &opcode, sizeof(int)) == -1)
            exit(EXIT_FAILURE);
        printf("Main thread: received message with opcode:%d\n", opcode);
        switch (opcode){
            // Mount new client pipe
            case 1:
                printf("Main thread: opcode 1\n");
                // Read server pipe
                if (read(fserver, client_pipe_path, sizeof(char[CLIENT_PIPE_PATH_SIZE])) == -1)
                    return -1;
                printf("Main thread: read pipename %s\n", client_pipe_path);
                // Handle client request
                if(addClientPipe(client_pipe_path) == -1)
                    exit(EXIT_FAILURE);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);    
            break;
            // Unmount Client
            case 2:
                // Read Server pipe
                printf("Main thread: opcode 2\n");
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("Main thread: read session_id %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_open
            case 3:
                // Read Server pipe
                printf("Main thread: opcode 3\n");
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("Main thread: read session_id %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->name, sizeof(char[40])) == -1)
                    exit(EXIT_FAILURE);
                printf("name: %s\n", buffer->name);
                if (read(fserver, &buffer->flags, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("flags: %d\n", buffer->flags);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs close
            case 4:
                // Read Server pipe
                printf("Main thread: opcode 4\n");
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("Message sent from session_id: %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("fhandle: %d\n", buffer->fhandle);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_write
            case 5:
                // Read Server pipe
                printf("Main thread: opcode 5\n");
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("Message sent from session_id: %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("fhandle: %d\n", buffer->fhandle);
                if (read(fserver, &buffer->len, sizeof(size_t)) == -1)
                    exit(EXIT_FAILURE);
                printf("len: %ld\n", buffer->len);
                buffer->buffer = malloc(sizeof(char[buffer->len]));
                if (read(fserver, buffer->buffer, sizeof(char[buffer->len])) == -1)
                    exit(EXIT_FAILURE);
                printf("Message sent to write: %s\n", buffer->buffer);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_read
            case 6:
                printf("Main thread: opcode 6\n");
                // Read Server pipe
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("message sent from session_id: %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("fhandle: %d\n", buffer->fhandle);
                if (read(fserver, &buffer->len, sizeof(size_t)) == -1)
                    exit(EXIT_FAILURE);
                printf("len: %ld\n", buffer->len);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_shutdown_after_all_closed()
            case 7:
                // Read Server pipe
                printf("Main thread: opcode 7\n");
                if (read(fserver, &session_id, sizeof(int)) == -1)
                    exit(EXIT_FAILURE);
                printf("Message sent from session_id: %d\n", session_id);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // Bad opcode
            default:
                // TO DO error checking
            break;
        }
    close (fserver);
    }

    unlink(pipename);

    return 0;
}