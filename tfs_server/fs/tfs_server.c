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
static buffer_entry buffer_entry_table[S];
// Client Pipe Paths table;
static char *client_pipes_table[S];
pthread_mutex_t client_session_table_lock;
static bool server_open = true;
static int session_id_shutdown;

void write_on_pipe(int fclient, void *buffer, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t ret = write(fclient, buffer + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}


void read_from_pipe(int fserver, void *buffer, size_t len) {
    size_t been_read = 0;
    while (been_read < len) {
        ssize_t ret = read(fserver, buffer + been_read, len - been_read);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        been_read += (size_t)ret;
    }
}


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
        // Temporary variables to store what the thread will write on pipe
        int return_value;
        ssize_t return_len;
        char *read_buffer;
        // Lock buffer

        if (pthread_mutex_lock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);
        // Wait for signal that we can read the buffer
        while (!(buffer->opcode > TFS_OP_CODE_NULL) && server_open){
            pthread_cond_wait(&buffer->cond, &buffer->lock);
        }
        // Read buffer
        switch (buffer->opcode){
            // tfs_mount
            case TFS_OP_CODE_MOUNT:
                if ((fclient = open(client_pipes_table[session_id], O_WRONLY)) < 0)
                    exit(EXIT_FAILURE);
                write_on_pipe(fclient, &session_id, TFS_MOUNT_RETURN_SIZE);
            break;
            // tfs_unmount
            case TFS_OP_CODE_UNMOUNT:
                removeClientPipe(session_id);
                return_value = 0;
                write_on_pipe(fclient, &return_value, TFS_UNMOUNT_RETURN_SIZE);
                close(fclient);
            break;
            // tfs_open
            case TFS_OP_CODE_OPEN:
                return_value = tfs_open(buffer->name, buffer->flags);
                write_on_pipe(fclient, &return_value, TFS_OPEN_RETURN_SIZE);
            break;
            //tfs_close
            case TFS_OP_CODE_CLOSE:
                return_value = tfs_close(buffer->fhandle);
                write_on_pipe(fclient, &return_value, TFS_CLOSE_RETURN_SIZE);
            break;
            // tfs_write
            case TFS_OP_CODE_WRITE:
                return_len = tfs_write(buffer->fhandle, buffer->buffer, buffer->len);
                free(buffer->buffer);
                write_on_pipe(fclient, &return_len, TFS_WRITE_RETURN_SIZE);
            break;
            // tfs_read
            case TFS_OP_CODE_READ:
                //Buffer to store read
                read_buffer = malloc(sizeof(char[buffer->len]));
                return_len = tfs_read(buffer->fhandle, read_buffer, buffer->len);
                // Buffer to store message for pipe
                void *return_buffer = malloc(TFS_LEN_SIZE + sizeof(char[return_len]));
                // Storing message in buffer
                memcpy(return_buffer, &return_len, TFS_LEN_SIZE);
                memcpy(return_buffer + TFS_LEN_SIZE, read_buffer, sizeof(char[return_len]));
                // Write on pipe
                write_on_pipe(fclient, return_buffer, TFS_LEN_SIZE + sizeof(char[return_len]));           
                free(read_buffer);
                free(return_buffer);
            break;
            // tfs_shutdown_after_all_closed
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                return_value = tfs_destroy_after_all_closed();
                if (return_value == 0){
                    server_open = false;
                    session_id_shutdown = session_id;
                }
                write_on_pipe(fclient, &return_value, TFS_SHUTDOWN_RETURN_SIZE);
            break;
            default:
            break;
        }
        buffer->opcode = TFS_OP_CODE_NULL;

        // Unlock buffer
        if (pthread_mutex_unlock(&buffer->lock) != 0)
            exit(EXIT_FAILURE);
    }
    close(fclient);
    return NULL;
}


int addClientPipe(char *client_pipe_path){
    if (pthread_mutex_lock(&client_session_table_lock) != 0)
            exit(EXIT_FAILURE);
    //Check entry with same client path
    for(int i = 0; i < S; i++){
        if (client_pipes_table[i] != NULL && strcmp(client_pipes_table[i], client_pipe_path) == 0){
            if (pthread_mutex_unlock(&client_session_table_lock) != 0)
                exit(EXIT_FAILURE);
            return i;
        }
    }
    // Check first free entry on pipes table
    for(int i = 0; i < S; i++){
        if(client_pipes_table[i] == NULL){
            // Store pipe path on pipe table
            client_pipes_table[i] = malloc(TFS_PIPENAME_SIZE);
            memcpy(client_pipes_table[i], client_pipe_path, TFS_PIPENAME_SIZE);
            if (pthread_mutex_unlock(&client_session_table_lock) != 0)
                exit(EXIT_FAILURE);
            return i;
        }
    }
    if (pthread_mutex_unlock(&client_session_table_lock) != 0)
            exit(EXIT_FAILURE);
    return -1;
}


void removeClientPipe(int session_id){
    if (pthread_mutex_lock(&client_session_table_lock) != 0)
            exit(EXIT_FAILURE);
    free(client_pipes_table[session_id]);
    client_pipes_table[session_id] = NULL;
    if (pthread_mutex_unlock(&client_session_table_lock) != 0)
            exit(EXIT_FAILURE);
    return;
}



int main(int argc, char **argv) {
    // Worker threads
    pthread_t worker_thread[S];
    pthread_mutex_init(&client_session_table_lock, NULL);


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
        // Temporary variables to store what we read from pipe
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
                read_from_pipe(fserver, client_pipe_path, TFS_PIPENAME_SIZE);
                // Handle client request
                session_id = addClientPipe(client_pipe_path);
                // Server Capacity is full, sends -1 to signal that mount wasn't successful
                if(session_id == -1){
                    int fclient;
                    if ((fclient = open(client_pipe_path, O_WRONLY)) < 0) {
                        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    write_on_pipe(fclient, &session_id, TFS_MOUNT_RETURN_SIZE);
                    close(fclient);
                    break;
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
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
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
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                read_from_pipe(fserver, &buffer->name, TFS_NAME_SIZE);
                read_from_pipe(fserver, &buffer->flags, TFS_FLAGS_SIZE);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs close
            case TFS_OP_CODE_CLOSE:
                // Read Server pipe
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                read_from_pipe(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_write
            case TFS_OP_CODE_WRITE:
                // Read Server pipe
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                read_from_pipe(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE);
                read_from_pipe(fserver, &buffer->len, TFS_LEN_SIZE);
                buffer->buffer = malloc(sizeof(char[buffer->len]));
                read_from_pipe(fserver, buffer->buffer, sizeof(char[buffer->len]));
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_read
            case TFS_OP_CODE_READ:
                // Read Server pipe
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
                // Get buffer
                buffer = &buffer_entry_table[session_id];
                // Lock Buffer
                if (pthread_mutex_lock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
                // Store data in buffer
                buffer->opcode = opcode;
                read_from_pipe(fserver, &buffer->fhandle, TFS_FHANDLE_SIZE);
                read_from_pipe(fserver, &buffer->len, TFS_LEN_SIZE);
                // Signal thread
                pthread_cond_signal(&buffer->cond);
                // Unlock buffer
                if (pthread_mutex_unlock(&buffer->lock) != 0)
                    exit(EXIT_FAILURE);
            break;
            // tfs_shutdown_after_all_closed()
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                // Read Server pipe
                read_from_pipe(fserver, &session_id, TFS_SESSIONID_SIZE);
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
    }


    for(int i = 0; i < S; i++){
        if (pthread_mutex_lock(&buffer_entry_table[i].lock) != 0)
            exit(EXIT_FAILURE);
        pthread_cond_signal(&buffer_entry_table[i].cond);
        if (pthread_mutex_unlock(&buffer_entry_table[i].lock) != 0)
            exit(EXIT_FAILURE);
    }

    for(int i = 0; i < S; i++){
        pthread_join(worker_thread[i], NULL);
    }

    close (fserver);
    unlink(pipename);

    return 0;
}