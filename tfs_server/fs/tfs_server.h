#ifndef TFS_SERVER
#define TFS_SERVER

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>


#define MAX_SESSIONS_AMOUNT 20
#define NAME_SIZE 40
#define SESSION_BUFFER_AMOUNT 1

/*
 * Buffer entry
 */
typedef struct {
    char opcode;
    char name[NAME_SIZE];
    int fhandle;
    int flags;
    size_t len;
    char *buffer;
} buffer_entry;

/* Function for the receiver thread that will read from the server pipe client requests */
void *serverPipeReader(void* arg);

/* Function for worker threads that will handle requests from clients */
void *requestHandler(void* arg);

/* Initializes server
 * Input:
 *      - receiver_thread
 *      - array with worker threads
 *      - server pipename
 */
void server_init(pthread_t *receiver_thread, pthread_t worker_thread[MAX_SESSIONS_AMOUNT], char *pipename);

/* Wait for all threads to finish and then destroys server
 * Input:
 *      - receiver_thread
 *      - array with worker threads
 *      - server pipename
 */
void server_destroy(pthread_t receiver_thread, pthread_t worker_thread[MAX_SESSIONS_AMOUNT], char *pipename);

/* Reads mount instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_mount(int fd);

/* Performs and writes return value of mount instruction to pipe
 * Input:
 *      - session id
 * Returns the client pipe file descriptor
 */
int write_mount(int session_id);

/* Reads unmount instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_unmount(int fd);

/* Performs and writes return value of unmount instruction to pipe
 * Input:
 *      - client pipe file descriptor
 *      - session id
 * Returns placeholder value of -1 for client file descriptor variable
 */
int write_unmount(int fd, int session_id);

/* Reads open instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_open(int fd);

/* Performs tfs_open and writes return value of open instruction to pipe
 * Input:
 *      - file handle
 *      - buffer
 */
void write_open(int fd, buffer_entry *buffer);

/* Reads close instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_close(int fd);

/* Performs tfs_close and writes return value of close instruction to pipe
 * Input:
 *      - file handle
 *      - buffer
 */
void write_close(int fd, buffer_entry *buffer);

/* Reads write instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_write(int fd);

/* Performs tfs_write and writes return value of write instruction to pipe
 * Input:
 *      - file handle
 *      - buffer
 */
void write_write(int fd, buffer_entry *buffer);

/* Reads read instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_read(int fd);

/* Performs tfs_read and writes return value of read instruction to pipe
 * Input:
 *      - file handle
 *      - buffer
 */
void write_read(int fd, buffer_entry *buffer);

/* Reads shutdown instruction from pipe
 * Input:
 *      - pipe file handle
 */
void read_shutdown(int fd);

/* Performs the server shutdown and writes return value of instruction to pipe
 * Input:
 *      - client pipe file descriptor
 */
void write_shutdown(int fd);

/* Writes to a pipe
 * Input:
 *      - pipe file handle
 *      - buffer to write from
 *      - amount to write from pipe
 */
void write_on_pipe(int fclient, void *buffer, size_t len);

/* Reads from a pipe
 * Input:
 *      - pipe file handle
 *      - buffer to read to
 *      - amount to read from pipe
 */
void read_from_pipe(int fserver, void *buffer, size_t len);

/* Adds a client pipe path to the table
 * Input:
 *      - session id from the pipe that will be removed
 * Returns 0 if successful, -1 otherwise.
 */
int addClientPipe(char *client_pipe_path);

/* Removes client pipe path from table
 * Input:
 *      - session id from the pipe that will be removed
 */
void removeClientPipe(int session_id);

/* Checks if the server is still open
 *
 * Returns 0 if true, -1 if false.
 */
int check_server_open();

/* Wrappers for error checking in pthread operations */

/* pthread_mutex_lock wrapper for error checking */
void lock_mutex(pthread_mutex_t *lock);

/* pthread_mutex_unlock wrapper for error checking */
void unlock_mutex(pthread_mutex_t *lock);

/* pthread_cond_signal wrapper for error checking */
void signal_cond(pthread_cond_t *cond);

/* pthread_cond_wait wrapper for error checking */
void wait_cond(pthread_cond_t *cond, pthread_mutex_t *lock);

/* Other function wrappers */

/* Increment a buffer counter in the buffer counter table */
void increment_buffer_counter(int session_id);

#endif // TFS_SERVER