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
static buffer_entry buffer_entry_table[MAX_SESSIONS_AMOUNT];
static pthread_mutex_t buffer_lock_table[MAX_SESSIONS_AMOUNT];
static pthread_cond_t buffer_cond_table[MAX_SESSIONS_AMOUNT];

// Client Pipe Paths table;
static char *client_pipes_table[MAX_SESSIONS_AMOUNT];
static pthread_mutex_t client_session_table_lock;
// Server Shutdown
static bool server_open = true;
static pthread_mutex_t server_lock;
static pthread_cond_t server_cond;


int main(int argc, char **argv) {
    // Receiver thread
    pthread_t receiver_thread;
    // Worker threads
    pthread_t worker_thread[MAX_SESSIONS_AMOUNT];

    //Argument Check
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    // Starting tfs_server
    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    server_init(&receiver_thread, worker_thread, pipename);

    // Wait for signal to shutdown the server
    lock_mutex(&server_lock);
    while (server_open){
        if (pthread_cond_wait(&server_cond, &server_lock) == -1)
            exit(EXIT_FAILURE);
    }
    unlock_mutex(&server_lock);

    server_destroy(receiver_thread, worker_thread, pipename);

    return 0;
}


void *serverPipeReader(void* arg){
    // Get arguments
    char *pipename = arg;

    int fserver;
    if ((fserver = open(pipename, O_RDONLY)) == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Wait for arguments
    while(1){
        // Get request opcode from pipe
        char opcode;
        ssize_t ret = read(fserver, &opcode, TFS_OPCODE_SIZE);
        // Reopen pipe if closed
        if (ret == 0) {
            close(fserver);
            if ((fserver = open(pipename, O_RDONLY)) == -1) {
                fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            ret = read(fserver, &opcode, TFS_OPCODE_SIZE);
        } else if (ret == -1) {
            fprintf(stderr, "[ERR]: opcode read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        switch (opcode){
            case TFS_OP_CODE_MOUNT:
                read_mount(fserver);
            break;
            case TFS_OP_CODE_UNMOUNT:
                read_unmount(fserver);
            break;
            case TFS_OP_CODE_OPEN:
                read_open(fserver);
            break;
            case TFS_OP_CODE_CLOSE:
                read_close(fserver);
            break;
            case TFS_OP_CODE_WRITE:
                read_write(fserver);
            break;
            case TFS_OP_CODE_READ:
                read_read(fserver);
            break;
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                read_shutdown(fserver);
            break;
            // Bad opcode
            default:
                exit(EXIT_FAILURE);
            break;
        }
    }
    close(fserver);
    return NULL;
}


void *requestHandler(void* arg){
    // Get arguments
    int session_id = *((int *) arg);
    free(arg);

    buffer_entry *buffer = &buffer_entry_table[session_id];
    pthread_mutex_t *lock = &buffer_lock_table[session_id];
    pthread_cond_t *cond = &buffer_cond_table[session_id];

    // Open client pipe
    int fclient = -1;

    // Start receiving requests;
    while(check_server_open()){
        // Lock buffer
        lock_mutex(lock);
        // Wait for signal that we can read the buffer
        while (!(buffer->opcode > TFS_OP_CODE_NULL) && check_server_open()){
            pthread_cond_wait(cond, lock);
        }
        switch (buffer->opcode){
            case TFS_OP_CODE_MOUNT:
                fclient = write_mount(session_id);
            break;
            case TFS_OP_CODE_UNMOUNT:
                fclient = write_unmount(fclient, session_id);
            break;
            case TFS_OP_CODE_OPEN:
                write_open(fclient, buffer);
            break;
            case TFS_OP_CODE_CLOSE:
                write_close(fclient, buffer);
            break;
            case TFS_OP_CODE_WRITE:
                write_write(fclient, buffer);
            break;
            case TFS_OP_CODE_READ:
                write_read(fclient, buffer);
            break;
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                write_shutdown(fclient);
            break;
            default:
                //
            break;
        }
        buffer->opcode = TFS_OP_CODE_NULL;
        unlock_mutex(lock);
    }

    if(fclient > 0)
        close(fclient);

    return NULL;
}


void server_init(pthread_t *receiver_thread, pthread_t worker_thread[MAX_SESSIONS_AMOUNT], char *pipename){
    // Initialize global mutexes and cond
    if(pthread_mutex_init(&client_session_table_lock, NULL) == -1)
        exit(EXIT_FAILURE);
    if(pthread_mutex_init(&server_lock, NULL) == -1)
        exit(EXIT_FAILURE);
    if(pthread_cond_init(&server_cond, NULL) == -1)
        exit(EXIT_FAILURE);

    // Initialize buffers' mutexes and conditional variables
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        if (pthread_mutex_init(&buffer_lock_table[i], NULL) == -1)
            exit(EXIT_FAILURE);
        if (pthread_cond_init(&buffer_cond_table[i], NULL) == -1)
            exit(EXIT_FAILURE);
    }

    // Start file system
    if (tfs_init() == -1)
        exit(EXIT_FAILURE);

    // Initialize client_pipe_table
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        client_pipes_table[i] = NULL;
    }

    // Create worker threads
    for (int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
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

    // Create thread that will read from server pipe
    if(pthread_create(receiver_thread, NULL, serverPipeReader, pipename) == -1){
        fprintf(stderr, "[ERR]: main thread creation failed");
        exit(EXIT_FAILURE);
    }

    return;
}


void server_destroy(pthread_t receiver_thread, pthread_t worker_thread[MAX_SESSIONS_AMOUNT], char *pipename){
    // Signal worker threads to quit
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        lock_mutex(&buffer_lock_table[i]);
        signal_cond(&buffer_cond_table[i]);
        unlock_mutex(&buffer_lock_table[i]);
    }

    // Wait for worker threads to quit
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        if (pthread_join(worker_thread[i], NULL) == -1)
            exit(EXIT_FAILURE);
    }
    // Close main thread
    if (pthread_cancel(receiver_thread) == -1)
        exit(EXIT_FAILURE);
    if (pthread_join(receiver_thread, NULL) == -1)
        exit(EXIT_FAILURE);
    // Destroy buffer mutexes
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        if (pthread_mutex_destroy(&buffer_lock_table[i]) == -1)
            exit(EXIT_FAILURE);
        if (pthread_cond_destroy(&buffer_cond_table[i]) == -1)
            exit(EXIT_FAILURE);
    }
    // Destroy free client pipes and destroy table lock
    if (pthread_mutex_destroy(&client_session_table_lock) == -1)
        exit(EXIT_FAILURE);
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        free(client_pipes_table[i]);
    }
    // Destroy server lock and cond
    if (pthread_mutex_destroy(&server_lock) == -1)
        exit(EXIT_FAILURE);
    if (pthread_cond_destroy(&server_cond) == -1)
        exit(EXIT_FAILURE);

    unlink(pipename);

    return;
}


void read_mount(int fd){
    char client_pipe_path[NAME_SIZE];
    // Read server pipe
    read_from_pipe(fd, client_pipe_path, TFS_PIPENAME_SIZE);
    // Handle client request
    int session_id = addClientPipe(client_pipe_path);
    // Server Capacity is full, sends -1 to signal that mount wasn't successful
    if(session_id == -1){
        int fclient;
        if ((fclient = open(client_pipe_path, O_WRONLY)) < 0) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        write_on_pipe(fclient, &session_id, TFS_MOUNT_RETURN_SIZE);
        close(fclient);
        return;
    }
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_MOUNT;
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


int write_mount(int session_id){
    int fd;
    // Open pipe
    if ((fd = open(client_pipes_table[session_id], O_WRONLY)) < 0)
        exit(EXIT_FAILURE);
    // Write return on pipe
    write_on_pipe(fd, &session_id, TFS_MOUNT_RETURN_SIZE);
    return fd;
}


void read_unmount(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_UNMOUNT;
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


int write_unmount(int fd, int session_id){
    // Remove client pipe path from table
    removeClientPipe(session_id);
    int return_value = 0;
    // Write return on pipe
    write_on_pipe(fd, &return_value, TFS_UNMOUNT_RETURN_SIZE);
    // Close pipe
    close(fd);
    return -1;
}


void read_open(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_OPEN;
    read_from_pipe(fd, &buffer->name, TFS_NAME_SIZE);
    read_from_pipe(fd, &buffer->flags, TFS_FLAGS_SIZE);
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


void write_open(int fd, buffer_entry *buffer){
    // Open file
    int return_value = tfs_open(buffer->name, buffer->flags);
    // Write return on pipe
    write_on_pipe(fd, &return_value, TFS_OPEN_RETURN_SIZE);
    return;
}


void read_close(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_CLOSE;
    read_from_pipe(fd, &buffer->fhandle, TFS_FHANDLE_SIZE);
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


void write_close(int fd, buffer_entry *buffer){
    // Close file
    int return_value = tfs_close(buffer->fhandle);
    // Write return on pipe
    write_on_pipe(fd, &return_value, TFS_CLOSE_RETURN_SIZE);
    return;
}


void read_write(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_WRITE;
    read_from_pipe(fd, &buffer->fhandle, TFS_FHANDLE_SIZE);
    read_from_pipe(fd, &buffer->len, TFS_LEN_SIZE);
    buffer->buffer = malloc(sizeof(char[buffer->len]));
    read_from_pipe(fd, buffer->buffer, sizeof(char[buffer->len]));
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


void write_write(int fd, buffer_entry *buffer){
    // Write on tfs
    ssize_t return_len = tfs_write(buffer->fhandle, buffer->buffer, buffer->len);
    free(buffer->buffer);
    // Write return on pipe
    write_on_pipe(fd, &return_len, TFS_WRITE_RETURN_SIZE);
    return;
}


void read_read(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_READ;
    read_from_pipe(fd, &buffer->fhandle, TFS_FHANDLE_SIZE);
    read_from_pipe(fd, &buffer->len, TFS_LEN_SIZE);
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


void write_read(int fd, buffer_entry *buffer){
    //Buffer to store read
    char *read_buffer = malloc(sizeof(char[buffer->len]));
    ssize_t return_len = tfs_read(buffer->fhandle, read_buffer, buffer->len);
    // Buffer to store message for pipe
    void *return_buffer = malloc(TFS_LEN_SIZE + sizeof(char[return_len]));
    // Storing message in buffer
    memcpy(return_buffer, &return_len, TFS_LEN_SIZE);
    memcpy(return_buffer + TFS_LEN_SIZE, read_buffer, sizeof(char[return_len]));
    // Write on pipe
    write_on_pipe(fd, return_buffer, TFS_LEN_SIZE + sizeof(char[return_len]));           
    free(read_buffer);
    free(return_buffer);
    return;
}


void read_shutdown(int fd){
    int session_id;
    // Read Server pipe
    read_from_pipe(fd, &session_id, TFS_SESSIONID_SIZE);
    // Lock Buffer
    lock_mutex(&buffer_lock_table[session_id]);
    // Get buffer
    buffer_entry *buffer = &buffer_entry_table[session_id];
    // Store data in buffer
    buffer->opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    // Signal thread
    signal_cond(&buffer_cond_table[session_id]);
    // Unlock buffer
    unlock_mutex(&buffer_lock_table[session_id]);
    return;
}


void write_shutdown(int fd){
    int return_value = tfs_destroy_after_all_closed();
    write_on_pipe(fd, &return_value, TFS_SHUTDOWN_RETURN_SIZE);
    lock_mutex(&server_lock);
    if (return_value == 0){
        server_open = false;
        signal_cond(&server_cond);
    }
    unlock_mutex(&server_lock);
    return;
}


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


int addClientPipe(char *client_pipe_path){
    lock_mutex(&client_session_table_lock);
    //Check entry with same client path
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        if (client_pipes_table[i] != NULL && strcmp(client_pipes_table[i], client_pipe_path) == 0){
            unlock_mutex(&client_session_table_lock);
            return -1;
        }
    }
    // Check first free entry on pipes table
    for(int i = 0; i < MAX_SESSIONS_AMOUNT; i++){
        if(client_pipes_table[i] == NULL){
            // Store pipe path on pipe table
            client_pipes_table[i] = malloc(TFS_PIPENAME_SIZE);
            memcpy(client_pipes_table[i], client_pipe_path, TFS_PIPENAME_SIZE);
            unlock_mutex(&client_session_table_lock);
            return i;
        }
    }
    unlock_mutex(&client_session_table_lock);
    return -1;
}


void removeClientPipe(int session_id){
    lock_mutex(&client_session_table_lock);
    free(client_pipes_table[session_id]);
    client_pipes_table[session_id] = NULL;
    unlock_mutex(&client_session_table_lock);
    return;
}


int check_server_open(){
    int return_value;
    lock_mutex(&server_lock);
    if(server_open)
        return_value = 1;
    else
        return_value = 0;
    unlock_mutex(&server_lock);
    return return_value;
}


void lock_mutex(pthread_mutex_t *lock){
    if(pthread_mutex_lock(lock) != 0)
        exit(EXIT_FAILURE);
    return;
}


void unlock_mutex(pthread_mutex_t *lock){
    if(pthread_mutex_unlock(lock) != 0)
        exit(EXIT_FAILURE);
    return;
}


void signal_cond(pthread_cond_t *cond){
    if (pthread_cond_signal(cond) == -1)
        exit(EXIT_FAILURE);
    return;
}


void wait_cond(pthread_cond_t *cond, pthread_mutex_t *lock){
    if (pthread_cond_wait(cond, lock) == -1)
        exit(EXIT_FAILURE);
    return;
}