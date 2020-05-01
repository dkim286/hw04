#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include <stddef.h>
#include <sys/types.h>

#include "disk.h"

#define MAX_FILES 64
#define MAX_FILENAME 16

#define FAT_UNUSED 0
#define FAT_EOF -1
#define FAT_RESERVED -2 

/*
 * Superblock -- superblock, first in line
 * All offsets are in blocks
 * alloc_table_offset: offset where allocation table starts
 * directory_offset: offset where root directory struct is stored
 * data_block_offset: offset where data block begins
 */
typedef struct {
    int fat_offset;
    int directory_offset;
    int data_block_offset;
} Superblock;


typedef struct {
    int table[DISK_BLOCKS];
} FAT;


/* Attribute -- an entry for 'Directory' struct mimicking the FAT filesystem
 * name: name of file
 * size: size of file in BYTES
 * offset: block offset where file's head block starts
 */
typedef struct {
    char name[MAX_FILENAME];
    int size;
    int offset;
} Attribute;


/* Directory -- represents the root dir
 * size: how many files are present
 * capacity: capacity of directory
 * attributes: each file entry, represented as an Attribute struct
 */
typedef struct {
    int size;
    Attribute attributes[MAX_FILES];
} Directory;

typedef struct {
    Superblock superblock;
    FAT fat;
    Directory directory;
} Disk;


/* filesystem api unctions */

int make_fs(char * disk_name);
int mount_fs(char * disk_name);
int umount_fs(char * disk_name);

int fs_open(char * name);
int fs_close(int fildes);
int fs_create(char * name);
int fs_delete(char * name);
int fs_read(int fildes, void * buf, size_t nbyte);
int fs_write(int fildes, void * buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

/* helpers */
void print_disk_struct();
int write_blocks(char * buf, int block_offset, int block_count);
int read_blocks(char * buf, int block_offset, int block_count);
void init_virt_disk();
int free_alloc_chain(int head);
int find_avail_alloc_entry();
int get_fildes_index(int fildes);
int get_file_blocksize(int fildes);
int get_eof_block_idx(int fildes);

#endif
