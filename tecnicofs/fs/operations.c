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
    pthread_rwlock_t *lock;
    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        lock = inode_lock_get(inum);
        pthread_rwlock_wrlock(lock);
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }
        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (inode_datablocks_delete(*inode) == -1) {
                    return -1;
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        lock = inode_lock_get(inum);
        pthread_rwlock_wrlock(lock);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }
    pthread_rwlock_unlock(lock);
    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    pthread_rwlock_t *lock = inode_lock_get(file->of_inumber);
    pthread_rwlock_wrlock(lock);

    if ((int)to_write < 0){
        return -1;
    }

    size_t writen = 0;
    int index = ((int)file->of_offset)/BLOCK_SIZE;

    for(; index < DIRECT_BLOCKS_QUANTITY && writen < to_write; index++){
        size_t to_write_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);

        if(to_write_in_block > to_write - writen)
            to_write_in_block = to_write - writen;

        if (inode->i_data_block[index] == -1) {
            inode->i_data_block[index] = data_block_alloc();
        }

        void *block = data_block_get(inode->i_data_block[index]);
        if (block == NULL) {
            return -1;
        }

        memcpy(block + file->of_offset%BLOCK_SIZE, buffer + writen, to_write_in_block);
        writen += to_write_in_block;
        file->of_offset += writen;
    }

    int *index_block = data_block_get(inode->i_index_block);
    if (index_block == NULL) {
        return -1;
    }

    if (index > DIRECT_BLOCKS_QUANTITY){
        index -= DIRECT_BLOCKS_QUANTITY;
    }
    else{
        index = 0;
    }

    for(; index < BLOCK_SIZE/sizeof(int) && writen < to_write; index++){
        size_t to_write_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);

        if(to_write_in_block > to_write - writen)
            to_write_in_block = to_write - writen;

        if (index_block[index] == -1) {
            index_block[index] = data_block_alloc();
        }

        void *block = data_block_get(index_block[index]);
        if (block == NULL) {
            return -1;
        }

        memcpy(block + file->of_offset%BLOCK_SIZE, buffer + writen, to_write_in_block);
        writen += to_write_in_block;
        file->of_offset += writen;
    }
    inode->i_size = writen;
    pthread_rwlock_unlock(lock);
    return (ssize_t)writen;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    pthread_rwlock_t *lock = inode_lock_get(file->of_inumber);
    pthread_rwlock_wrlock(lock);

    /* Determine how many bytes to read */
    size_t to_read = len;

    size_t read = 0;
    int index = (int)file->of_offset/BLOCK_SIZE;

    for(; index < DIRECT_BLOCKS_QUANTITY && read < to_read; index++){
        size_t to_read_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);

        if(to_read_in_block > to_read-read)
            to_read_in_block = to_read-read;

        void *block = data_block_get(inode->i_data_block[index]);
        if (block == NULL) {
            return -1;
        }

        memcpy(buffer + read, block + file->of_offset%BLOCK_SIZE, to_read_in_block);
        read += to_read_in_block;
        file->of_offset += read;
    }

    int *index_block = data_block_get(inode->i_index_block);
    if (index_block == NULL) {
        return -1;
    }

    index -= DIRECT_BLOCKS_QUANTITY;

    for(; index < BLOCK_SIZE/sizeof(int) && read < to_read; index++){
        size_t to_read_in_block = BLOCK_SIZE - (file->of_offset%BLOCK_SIZE);

        if(to_read_in_block > to_read)
            to_read_in_block = to_read;

        void *block = data_block_get(index_block[index]);
        if (block == NULL) {
            return -1;
        }

        memcpy(buffer + read, block + file->of_offset%BLOCK_SIZE, to_read_in_block);
        read += to_read_in_block;
        file->of_offset += read;
    }

    pthread_rwlock_unlock(lock);

    return (ssize_t)to_read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
    
    /* Checks if the path name is valid */
    if (tfs_lookup(source_path) < 0) {
        return -1;
    }

    int source_inumber = tfs_open(source_path, TFS_O_CREAT);
    if (source_inumber == -1){
        return -1;
    }

    inode_t *inode = inode_get(source_inumber);

    if (inode == NULL){
        return -1;
    }
    size_t source_size = inode->i_size;

    void *buffer = malloc(source_size);

    ssize_t size = tfs_read(source_inumber, buffer, source_size);
    if (size < 0) {
        return -1;
    }

    int dest_file = open(dest_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if(dest_file < 0){
        return -1;
    }   

    if (write(dest_file, buffer, (size_t)size) != size){
        return -1;
    }

    if(tfs_close(source_inumber) < 0){
        return -1;
    }
    
    if(close(dest_file) < 0){
        return -1;
    }

    free(buffer);

    return 0;
}