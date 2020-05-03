#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "filesystem.h"
#include "descriptor.c"

extern Descriptor descriptors[];
int descriptor_size = 0;

/* block size vars */
const int SUPERBLOCK_BLOCK_SIZE
    = (sizeof(Superblock) + BLOCK_SIZE - 1) / BLOCK_SIZE;
const int FAT_BLOCK_SIZE 
    = (sizeof(FAT) + BLOCK_SIZE - 1) / BLOCK_SIZE;
const int DIRECTORY_BLOCK_SIZE 
    = (sizeof(Directory) + BLOCK_SIZE - 1) / BLOCK_SIZE;

/* disk structures ---------------------------------------------------------- */
static char * disk;

static Superblock * super;
static FAT * fat;
static Directory * dir;
static char * data;
/* -------------------------------------------------------------------------- */

static int virt_disk_active = 0;

int make_fs(char * disk_name)
{
	if (make_disk(disk_name) < 0)
		return -1;
	if (open_disk(disk_name) < 0)
		return -1;

    if (virt_disk_active != 1)
        init_virt_disk();

    // reserve 0 to data_block_offset in FAT
    for (int i = 0; i < super->data_block_offset; i++)
        fat->table[i] = FAT_RESERVED;

    // create a file real quick
    fs_create("example");

    if (write_blocks(disk, 0, DISK_BLOCKS) < 0)
        return -1;
	
    if (close_disk(disk_name) < 0)
        exit(1);

    return 0;
}

int mount_fs(char * disk_name)
{
    if (open_disk(disk_name) < 0)
        return -1;

    if (virt_disk_active != 1)
        init_virt_disk();
    
    if (read_blocks(disk, 0, DISK_BLOCKS) < 0)
        return -1;

    return 0;
}


int umount_fs(char * disk_name)
{
    if (write_blocks(disk, 0, DISK_BLOCKS) < 0)
        return -1;

    if (close_disk(disk_name) < 0)
        return -1;

    free(disk);

    return 0;
}

int fs_open(char * name)
{
    int idx = 0;
    Descriptor * desc;
    Attribute * attr;

    if (descriptor_size == MAX_OPEN_FILES)
    {
        printf("fs_open: can't open more files\n");
        return -1;
    }

    while (idx < dir->size && strcmp(name, dir->attributes[idx].name) != 0)
    {
        idx++;
    }

    if (idx == dir->size)
    {
        printf("fs_open: file not found: %s\n", name);
        return -1;
    }
    attr = &dir->attributes[idx];

    // find an empty descriptor spot
    idx = 0;
    while (idx < descriptor_size 
            && descriptors[idx].descriptor != DESCRIPTOR_UNUSED)
    {
        idx++;
    }
    desc = &descriptors[idx];

    // finally, create a descriptor
    desc->descriptor = idx;
    desc->ptr = disk + attr->offset * BLOCK_SIZE;
    desc->attr = attr;
    descriptor_size++;

    return desc->descriptor;
}

int fs_close(int fildes)
{
    int idx = get_fildes_index(fildes);
    if (idx < 0)
    {
        printf("fs_close: file with descriptor %d doesn't exist\n", fildes);
        return -1;
    }
    descriptor_size--;

    if (descriptor_size == 0)
        return 0;

    descriptors[idx] = descriptors[descriptor_size];

    return 0;
}

int fs_create(char * name)
{
    int found = 0;
    int dir_idx = 0;
    int name_idx = 0;
    int fat_idx = super->fat_offset;
    Attribute * attrib;

    if (dir->size == MAX_FILES)
    {
        printf("Max file count reached (64)\n");
        return -1;
    }

    while (name[name_idx] != '\0')
        name_idx++;

    if (name_idx >= MAX_FILENAME)
    {
        printf("Filename can't be longer than 15 characters.\n");
        return -1;
    }

    while (!found && dir_idx < dir->size)
    {
        if (strcmp(dir->attributes[dir_idx].name, name) == 0)
            found = 1;
        else
            dir_idx++;
    }
    if (found)
    {
        printf("File already exists\n");
        return -1;
    }

    // find an empty FAT entry
    fat_idx = find_avail_alloc_entry();
    if (fat_idx < 0)
    {
        printf("Not enough space\n");
        return -1;
    }

    // finally, create a file attrib entry
    attrib = &dir->attributes[dir->size];
    strncpy(attrib->name, name, MAX_FILENAME);
    attrib->size = 0;
    attrib->offset = fat_idx;

    fat->table[fat_idx] = FAT_EOF;

    dir->size++;

    return 0;
}

int fs_delete(char * name)
{
    int idx = 0;    /* index of file with matching name */
   
    /* find file within descriptors (is it open?) */
    while (idx < descriptor_size 
            && strcmp(name, descriptors[idx].attr->name) != 0)
    {
        idx++;
    }

    if (idx < descriptor_size)
    {
        printf("fs_delete: can't delete, file is open: %s\n", name);
        return -1;
    }

    /* find file within dir structure */
    idx = 0;
    while (idx < dir->size && strcmp(name, dir->attributes[idx].name) != 0)
    {
        idx++;
    }

    if (idx == dir->size)
    {
        printf("fs_delete: file not found: %s\n", name);
        return -1;
    }

    // finally "delete" the file
    dir->size--;
    free_alloc_chain(dir->attributes[idx].offset);
    if (dir->size == 0)
    {
        return 0;
    }
    dir->attributes[idx] = dir->attributes[dir->size];
    return 0;
}

int fs_read(int fildes, void * buf, size_t nbyte)
{
    size_t block_offset,    /* current block's offset */
           bytes_to_read    /* bytes readable from the block, up to nbytes */,
           bytes_r = 0;     /* bytes read from recursive calls, if any */
    int fat_idx,
        idx = get_fildes_index(fildes);
    char * destination = (char *) buf;

    if (idx < 0)
        return -1;

    block_offset = (long unsigned)(descriptors[idx].ptr - disk) / BLOCK_SIZE;
    bytes_to_read = (long unsigned)disk + (block_offset + 1) * BLOCK_SIZE 
        - (long unsigned)(descriptors[idx].ptr);
   
    // only read up to filesize
    if (nbyte > (size_t)descriptors[idx].attr->size)
        nbyte = (size_t)descriptors[idx].attr->size;

    // only read up to nbytes
    if (bytes_to_read > nbyte)
        bytes_to_read = nbyte;
    
    nbyte -= bytes_to_read;

    memcpy(destination, descriptors[idx].ptr, bytes_to_read);
    descriptors[idx].ptr += bytes_to_read;
    destination += bytes_to_read;

    if (nbyte == 0)
        return bytes_to_read;
   
    // the entire block has been read: find next block
    if (descriptors[idx].ptr == disk + (block_offset + 1) * BLOCK_SIZE)
    {
        fat_idx = fat->table[block_offset];

        // no more blocks: return bytes of what's been read
        if (fat_idx == FAT_EOF)
            return bytes_to_read;

        descriptors[idx].ptr = disk + fat_idx * BLOCK_SIZE;
    }

    bytes_r = fs_read(fildes, destination, nbyte);
    return bytes_to_read + bytes_r;
}

int fs_write(int fildes, void * buf, size_t nbyte)
{
    size_t block_offset,    /* current block's offset */
           bytes_to_fill,   /* bytes that can fit in the block, up to nbytes */
           bytes_r = 0;     /* bytes written from recursive calls, if any */
    int fat_idx,
        bytes_delta,
        idx = get_fildes_index(fildes);
    char * source = (char *) buf;

    if (idx < 0)
        return -1;

    block_offset = (long unsigned)(descriptors[idx].ptr - disk) / BLOCK_SIZE;
    bytes_to_fill = (long unsigned)disk + (block_offset + 1) * BLOCK_SIZE 
        - (long unsigned)(descriptors[idx].ptr);

    // write only up to nbytes
    if (bytes_to_fill > nbyte)
        bytes_to_fill = nbyte;
        
    nbyte -= bytes_to_fill;

    // write to virt disk and advance pointer forward
    memcpy(descriptors[idx].ptr, source, bytes_to_fill);
    descriptors[idx].ptr += bytes_to_fill;

    /* if at EOF block AND writing past file limit, increase filesize */
    if (block_offset == (size_t)get_eof_block_idx(fildes))
    {
        bytes_delta = bytes_to_fill - descriptors[idx].attr->size % BLOCK_SIZE;
        if (bytes_delta > 0)
            descriptors[idx].attr->size += bytes_delta;
    }

    // file ptr is at end of block: move it to next available block
    if (descriptors[idx].ptr == disk + (block_offset + 1) * BLOCK_SIZE)
    {
        fat_idx = fat->table[block_offset];
        if (fat_idx == FAT_EOF)
        {
            fat_idx = find_avail_alloc_entry();

            // no more blocks avail, ret bytes written up to now
            if (fat_idx < 0)
                return bytes_to_fill;

            // extend current FAT idx to the next chain
            fat->table[block_offset] = fat_idx;
            fat->table[fat_idx] = FAT_EOF;
            descriptors[idx].ptr = disk + fat_idx * BLOCK_SIZE;
        }
    }

    if (nbyte > 0)
        bytes_r = fs_write(fildes, source + bytes_to_fill, nbyte);
    return bytes_to_fill + bytes_r;
}

int fs_get_filesize(int fildes)
{
    int idx = get_fildes_index(fildes);
    if (idx < 0)
        return -1;
    return descriptors[idx].attr->size;
}

int fs_lseek(int fildes, off_t offset)
{
    int blocks, 
        fat_idx,
        idx = get_fildes_index(fildes);

    if (idx < 0)
        return -1;
    if (descriptors[idx].attr->size < offset)
        return -1;
    if (offset < 0)
        return -1;

    // seek to beginning if offset = 0
    if (offset == 0)
    {
        descriptors[idx].ptr 
            = disk + descriptors[idx].attr->offset * BLOCK_SIZE;
        return 0;
    }

    blocks = get_file_blocksize(fildes);

    // file's FAT table index for its final block
    fat_idx = descriptors[idx].attr->offset;
    for (int i = 0; i < blocks - 1; i++)
    {
        fat_idx = fat->table[fat_idx];
    }

    // seek to that location
    descriptors[idx].ptr = disk + fat_idx * BLOCK_SIZE + offset % BLOCK_SIZE;

    return 0;
}

int fs_truncate(int fildes, off_t length)
{
    int blocks,
        fat_idx,
        eof_idx,
        idx = get_fildes_index(fildes);
    char * file_ptr;

    if (idx < 0)
        return -1;
    if (descriptors[idx].attr->size < length)
        return -1;
   
    // truncated file's block footprint
    blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
   
    // find truncated file's FAT table index for its final block
    fat_idx = descriptors[idx].attr->offset;
    for (int i = 1; i < blocks - 1; i++)
    {
        fat_idx = fat->table[fat_idx];
    }

    // truncated length is EOF? ret 0
    if (fat->table[fat_idx] == FAT_EOF)
        return 0;

    // else, set truncated block's end as EOF and free the rest
    eof_idx = fat_idx;
    fat_idx = fat->table[fat_idx];
    free_alloc_chain(fat_idx);
    fat->table[eof_idx] = FAT_EOF;
    memset(disk + eof_idx * BLOCK_SIZE + length % BLOCK_SIZE, 0, 
            BLOCK_SIZE - (length % BLOCK_SIZE));
    descriptors[idx].attr->size = length;

    // ptr to final byte of truncated file
    file_ptr = disk + fat_idx * BLOCK_SIZE 
        + descriptors[idx].attr->size % BLOCK_SIZE;

    // reset file ptr if it's pointing past the truncated end
    if (file_ptr < descriptors[idx].ptr)
        return fs_lseek(fildes, descriptors[idx].attr->size);
    return 0;
}





/* helpers ------------------------------------------------------------------ */

void print_disk_struct()
{
    Attribute * attrib = &dir->attributes[0];

    printf("----------\n");
    printf("Superblock: fat=%d dir=%d data=%d \n", 
            super->fat_offset, 
            super->directory_offset, 
            super->data_block_offset);

    printf("FAT (first 20): ");
    for (int i = 0; i < 20; i++)
    {
        printf("%d ", fat->table[i]);
    }
    if (dir->size == 0)
        printf("\nNo files present\n");
    else
        printf("\nFirst file: name=%s size=%d offset=%d\n",
                attrib->name,
                attrib->size,
                attrib->offset);
    printf("First 50 bytes of data block: |");
    for (int i = 0; i < 50; i++)
    {
        printf("%c", data[i]);
    }
    printf("|\n");
    printf("----------\n");
}

int write_blocks(char * buf, int block_offset, int block_count)
{
    int block_end = block_count + block_offset;
    while (block_offset < block_end)
    {
        if (block_write(block_offset, buf) < 0)
            return -1;
        buf += BLOCK_SIZE;
        block_offset++;
    }
    return block_offset;
}
int read_blocks(char * buf, int block_offset, int block_count)
{
    int block_end = block_count + block_offset;
    while (block_offset < block_end)
    {
        if (block_read(block_offset, buf) < 0)
            return -1;
        buf += BLOCK_SIZE;
        block_offset++;
    }
    return block_offset;
}

void init_virt_disk()
{
    disk = malloc(DISK_BLOCKS * BLOCK_SIZE);
    memset (disk, 0, DISK_BLOCKS * BLOCK_SIZE);

    // init superblock
    super = (Superblock*) disk;
    super->fat_offset = SUPERBLOCK_BLOCK_SIZE;
    super->directory_offset = SUPERBLOCK_BLOCK_SIZE + FAT_BLOCK_SIZE;
    super->data_block_offset = 
        SUPERBLOCK_BLOCK_SIZE + FAT_BLOCK_SIZE  + DIRECTORY_BLOCK_SIZE;
    
    fat = (FAT *) (disk + super->fat_offset * BLOCK_SIZE);
    dir = (Directory*) (disk + super->directory_offset * BLOCK_SIZE);
    data = disk + super->data_block_offset * BLOCK_SIZE;
    
    // mark filesystem blocks as reserved
    for (int i = 0; i < super->data_block_offset; i++)
    {
        fat->table[i] = FAT_RESERVED;
    }

    virt_disk_active = 1;
}

int free_alloc_chain(int head)
{
    int idx;

    /* this shouldn't happen */
    if (head > DISK_BLOCKS || head == FAT_RESERVED)
        return -1;

    idx = fat->table[head];
    memset(disk + head * BLOCK_SIZE, 0, BLOCK_SIZE);

    /* either unused or probably hit the end of the chain */
    if (idx == FAT_UNUSED)
    {
        return 0;
    }

    fat->table[head] = FAT_UNUSED;
    if (idx != FAT_EOF)
        return free_alloc_chain(idx);
    else
        return 0;
}

int find_avail_alloc_entry()
{
    int fat_idx = super->data_block_offset;
    while (fat_idx < DISK_BLOCKS
            && fat->table[fat_idx] != FAT_UNUSED)
    {
        fat_idx++;
    }
    
    if (fat_idx == DISK_BLOCKS)
        return -1;
    return fat_idx;
}
int get_fildes_index(int fildes)
{
    int i = 0;

    if (descriptor_size == 0)
        return -1;

    while (i < descriptor_size && descriptors[i].descriptor != fildes)
    {
        i++;
    }

    if (i == descriptor_size)
        return -1;
    return i;
}
int get_file_blocksize(int fildes)
{
    int idx = get_fildes_index(fildes);

    if (idx < 0)
        return -1;

    return (descriptors[idx].attr->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}
int get_eof_block_idx(int fildes)
{
    int block_idx,
        idx = get_fildes_index(fildes);

    if (idx < 0)
        return -1;

    block_idx = descriptors[idx].attr->offset;
    while (fat->table[block_idx] != FAT_EOF)
    {
        block_idx = fat->table[block_idx];
    }
    return block_idx;
}
