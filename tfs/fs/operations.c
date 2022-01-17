#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;
    int append_flag = 0;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        pthread_rwlock_wrlock(inode_lock_get(inum));   
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }
        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (inode_datablocks_erase(*inode) == -1) {
                    pthread_rwlock_unlock(inode_lock_get(inum));
                    return -1;
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
            append_flag = 1;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        pthread_rwlock_wrlock(inode_lock_get(inum));
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            pthread_rwlock_unlock(inode_lock_get(inum));
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }
    pthread_rwlock_unlock(inode_lock_get(inum));
    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset, append_flag);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    /* Get the open file entry */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* Checking if the amount to write is valid */
    if ((int)to_write < 0){
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* 
    Ensuring we write in the end of the file if it was opened with the TSF_O_APPEND 
    flag, in case another thread also opened the file with different flag
    */
    if (file->of_append_flag == 1){
        file->of_offset = inode->i_size;
    }

    /* Locking the inode */
    pthread_rwlock_t *lock = inode_lock_get(file->of_inumber);
    pthread_rwlock_wrlock(lock);

    size_t writen = 0;
    /* Getting the index of the inode data blocks to start writing */
    int index = ((int)file->of_offset)/BLOCK_SIZE;

    for(; index < DIRECT_BLOCKS_QUANTITY && writen < to_write; index++){
        /* Checking the remaining space to write in the block */
        size_t to_write_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);
        if(to_write_in_block > to_write - writen)
            to_write_in_block = to_write - writen;
        /* Allocing and writing in the block */
        if (inode->i_data_block[index] == -1) {
            inode->i_data_block[index] = data_block_alloc();
        }
        void *block = data_block_get(inode->i_data_block[index]);
        if (block == NULL) {
            /* Return how much we have already written in case of error*/
            pthread_rwlock_unlock(lock);
            return (ssize_t)writen;
        }
        memcpy(block + file->of_offset%BLOCK_SIZE, buffer + writen, to_write_in_block);
        /* Updating offset and how much we have already writen*/
        writen += to_write_in_block;
        file->of_offset += writen;
    }

    /* Getting the index block */
    int *index_block = data_block_get(inode->i_index_block);
    if (index_block == NULL) {
        pthread_rwlock_unlock(lock);
        return -1;
    }

    /* Updating index for indirect blocks */
    if (index > DIRECT_BLOCKS_QUANTITY){
        index -= DIRECT_BLOCKS_QUANTITY;
    } else {
        index = 0;
    }

    for(; index < BLOCK_SIZE/sizeof(int) && writen < to_write; index++){
        /* Checking the remaining space to write in the block */
        size_t to_write_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);
        if(to_write_in_block > to_write - writen)
            to_write_in_block = to_write - writen;
        /* Allocing and writing in the block */
        if (index_block[index] == -1) {
            index_block[index] = data_block_alloc();
        }
        void *block = data_block_get(index_block[index]);
        if (block == NULL) {
            /* Return how much we have already written in case of error*/
            pthread_rwlock_unlock(lock);
            return (ssize_t)writen;
        }
        memcpy(block + file->of_offset%BLOCK_SIZE, buffer + writen, to_write_in_block);
        /* Updating offset and how much we have already writen*/
        writen += to_write_in_block;
        file->of_offset += writen;
    }

    /* If we wrote a bigger file than what was previously written update the size */
    if (inode->i_size < file->of_offset){
        inode->i_size = file->of_offset;
    }

    /* Unlocking the inode*/
    pthread_rwlock_unlock(lock);
    return (ssize_t)writen;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* Getting the file entry */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* Getting the inode*/
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Locking the inode*/
    pthread_rwlock_t *lock = inode_lock_get(file->of_inumber);
    pthread_rwlock_wrlock(lock);

    /* Ensuring we are not reading stuff off the file */
    if (inode->i_size < file->of_offset){
        file->of_offset = inode->i_size;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (len < to_read){
        to_read = len;
    }

    size_t read = 0;

    /* Getting the index of the inode data blocks to start reading */
    int index = (int)file->of_offset/BLOCK_SIZE;

    for(; index < DIRECT_BLOCKS_QUANTITY && read < to_read; index++){
        /* Checking the remaining space to read in the block */
        size_t to_read_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);
        if(to_read_in_block > to_read-read)
            to_read_in_block = to_read-read;
        /* Getting and reading the data block */
        void *block = data_block_get(inode->i_data_block[index]);
        if (block == NULL) {
            /* Return how much we have already written in case of error*/
            pthread_rwlock_unlock(lock);
            return (ssize_t)read;
        }
        memcpy(buffer + read, block + file->of_offset%BLOCK_SIZE, to_read_in_block);
        /* Updating how much we have read */
        read += to_read_in_block;
        file->of_offset += read;
    }

    /* Getting the indirect block */
    int *index_block = data_block_get(inode->i_index_block);
    if (index_block == NULL) {
        pthread_rwlock_unlock(lock);
        return -1;
    }

    /* Updating index for indirect blocks */
    if (index > DIRECT_BLOCKS_QUANTITY){
        index -= DIRECT_BLOCKS_QUANTITY;
    } else {
        index = 0;
    }
    for(; index < BLOCK_SIZE/sizeof(int) && read < to_read; index++){
        /* Checking the remaining space to read in the block */
        size_t to_read_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);
        if(to_read_in_block > to_read)
            to_read_in_block = to_read;
        /* Getting and reading the data block */
        void *block = data_block_get(index_block[index]);
        if (block == NULL) {
            /* Return how much we have already read in case of error*/
            pthread_rwlock_unlock(lock);
            return (ssize_t)read;
        }
        memcpy(buffer + read, block + file->of_offset%BLOCK_SIZE, to_read_in_block);
        /* Updating how much we have read */
        read += to_read_in_block;
        file->of_offset += read;
    }
    /* Unlocking the inode and returning how much we have read */
    pthread_rwlock_unlock(lock);
    return (ssize_t)read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
    /* Checks if the path name is valid */
    if (tfs_lookup(source_path) < 0) {
        return -1;
    }
    /* Getting the inumber and inode */
    int source_inumber = tfs_open(source_path, 0);
    if (source_inumber == -1){
        return -1;
    }
    inode_t *inode = inode_get(source_inumber);
    if (inode == NULL){
        return -1;
    }
    /* Getting the buffer */
    size_t source_size = inode->i_size;
    void *buffer = malloc(source_size);
    /* Getting the file in the tfs */
    ssize_t size = tfs_read(source_inumber, buffer, source_size);
    if (size < 0) {
        return -1;
    }
    /* Writing the file in the external fs*/
    int dest_file = open(dest_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if(dest_file < 0){
        return -1;
    }
    if (write(dest_file, buffer, (size_t)size) != size){
        return -1;
    }
    /* Close file */
    if(tfs_close(source_inumber) < 0){
        return -1;
    }
    if(close(dest_file) < 0){
        return -1;
    }
    /* Free buffer */
    free(buffer);

    return 0;
}