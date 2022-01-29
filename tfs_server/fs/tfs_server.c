#include "operations.h"
#include "tfs_server.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>



// Buffers
buffer_entry buffer_entry_table[S];
// Client Pipe Paths table;
char *client_pipes_table[S];
bool server_open = true;

void *requestHandler(void* arg){
    // Get arguments
    int session_id = *((int *) arg);
    free(arg);
    buffer_entry *buffer = &buffer_entry_table[session_id];

    // Initialize mutex and conditional variable
    pthread_mutex_init(&buffer->lock, NULL);
    pthread_cond_init(&buffer->cond, NULL);

    // Open client pipe
    int fclient = -1;

    // Start receiving requests;
    while(server_open){
        int return_value;
        ssize_t return_len;
        char *read_buffer = "";
        // Lock buffer
        if (pthread_mutex_lock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);

        // Wait for signal that we can read the buffer
        while (!(buffer->opcode >= TFS_OP_CODE_NULL)){
            pthread_cond_wait(&buffer->cond, &buffer->lock);
        }
        
        // Read buffer
        switch (buffer->opcode){
            // tfs_mount
            case TFS_OP_CODE_MOUNT:
                if ((fclient = open(client_pipes_table[session_id], O_WRONLY)) < 0)
                    exit(EXIT_FAILURE);
                if (write(fclient, &session_id, TFS_MOUNT_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
            // tfs_unmount
            case TFS_OP_CODE_UNMOUNT:
                client_pipes_table[session_id] = NULL;
                return_value = 0;
                if (write(fclient, &return_value, TFS_UNMOUNT_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                close(fclient);
            break;
            // tfs_open
            case TFS_OP_CODE_OPEN:
                return_value = tfs_open(buffer->name, buffer->flags);
                if (write(fclient, &return_value, TFS_OPEN_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
            //tfs_close
            case TFS_OP_CODE_CLOSE:
                return_value = tfs_close(buffer->fhandle);
                if (write(fclient, &return_value, TFS_CLOSE_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
            // tfs_write
            case TFS_OP_CODE_WRITE:
                return_len = tfs_write(buffer->fhandle, buffer->buffer, buffer->len);
                free(buffer->buffer);
                if (write(fclient, &return_len, TFS_WRITE_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
            // tfs_read
            case TFS_OP_CODE_READ:
                read_buffer = malloc(sizeof(char[buffer->len]));     
                return_len = tfs_read(buffer->fhandle, read_buffer, buffer->len);
                void *return_buffer = malloc(TFS_LEN_SIZE + sizeof(char[return_len]));
                size_t read_buffer_size = 0;

                memcpy(return_buffer + read_buffer_size, &return_len, TFS_LEN_SIZE);
                read_buffer_size += TFS_LEN_SIZE;
                memcpy(return_buffer + read_buffer_size, read_buffer, sizeof(char[return_len]));
                read_buffer_size += sizeof(char[return_len]);
                if (write(fclient, return_buffer, read_buffer_size) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }                           
                free(read_buffer);
                free(return_buffer);
            break;
            // tfs_shutdown_after_all_closed
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                return_value = tfs_destroy_after_all_closed();
                if (write(fclient, &return_value, TFS_SHUTDOWN_RETURN_SIZE) < 0) {
                    fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
            default:
                // deal with errors
            break;
        }
        buffer->opcode = TFS_OP_CODE_NULL;

        // Unlock buffer
        if (pthread_mutex_unlock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);
    }
    close(fclient);

    pthread_exit(NULL);
}


int addClientPipe(char *client_pipe_path){
    //Check entry with same client path
    for(int i = 0; i < S; i++){
        if (client_pipes_table[i] != NULL && strcmp(client_pipes_table[i], client_pipe_path) == 0){
            return i;
        }
    }
    // Check first free entry on pipes table
    for(int i = 0; i < S; i++){
        if(client_pipes_table[i] == NULL){
            // Store pipe path on pipe table
            client_pipes_table[i] = client_pipe_path;
            return i;
        }
    }
    return -1;
}



int main(int argc, char **argv) {
    // Worker threads
    pthread_t worker_thread[S];

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
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        if(pthread_create(&worker_thread[i], NULL, requestHandler, (void*)arg) == -1){
            fprintf(stderr, "[ERR]: thread creation failed: %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
        
    // Create server pipe
    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(pipename, 0777) != 0){
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int fserver;
    if ((fserver = open (pipename, O_RDONLY)) == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Wait for arguments
    while(server_open){
        char opcode;
        int session_id;
        char client_pipe_path[NAME_SIZE];
        buffer_entry *buffer;
        // Get request opcode from pipe
        ssize_t ret = read(fserver, &opcode, TFS_OPCODE_SIZE);
        if (ret == 0) {
            // ret == 0 signals EOF
            close(fserver);
            if ((fserver = open (pipename, O_RDONLY)) == -1) {
                fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            ret = read(fserver, &opcode, TFS_OPCODE_SIZE);
        } else if (ret == -1) {
            // ret == -1 signals error
            fprintf(stderr, "[ERR]: opcode read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        switch (opcode){
            // Mount new client pipe
            case TFS_OP_CODE_MOUNT:
                // Read server pipe
                if (read(fserver, client_pipe_path, TFS_PIPENAME_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Handle client request
                session_id = addClientPipe(client_pipe_path);
                if(session_id == -1){
                    fprintf(stderr, "[ERR]: addClientpipe failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
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
            case TFS_OP_CODE_UNMOUNT:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
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
            case TFS_OP_CODE_OPEN:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->name, TFS_NAME_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (read(fserver, &buffer->flags, TFS_FLAGS_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs close
            case TFS_OP_CODE_CLOSE:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_write
            case TFS_OP_CODE_WRITE:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (read(fserver, &buffer->len, TFS_LEN_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                buffer->buffer = malloc(sizeof(char[buffer->len]));
                if (read(fserver, buffer->buffer, sizeof(char[buffer->len])) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_read
            case TFS_OP_CODE_READ:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                if (read(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (read(fserver, &buffer->len, TFS_LEN_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_shutdown_after_all_closed()
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                // Read Server pipe
                if (read(fserver, &session_id, TFS_SESSIONID_SIZE) == -1){
                    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                server_open = false;            
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
    }

    for(int i = 0; i < S; i++){
        pthread_join(worker_thread[i], NULL);
    }

    close (fserver);
    unlink(pipename);

    return 0;
}