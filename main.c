#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "filesystem.h"

static const int DISKNAME_LEN = 100;
static const char * DISK_DIR = "./disks/";

int get_diskname(char * name);
int list_disks();

int main()
{
    char buf[DISKNAME_LEN - strlen(DISK_DIR)];
    char diskname[DISKNAME_LEN];
    int create = 0;
    
    create = get_diskname(buf);
    strcpy(diskname, DISK_DIR);
    strncat(diskname, buf, DISKNAME_LEN);

    if (create)
    {
        if (make_fs(diskname) < 0)
            return 1;
    }

    if (mount_fs(diskname) < 0)
        return 1;

    // test it out real quick
    fs_create("example");
    int d = fs_open("example");
    char buffer[13] = "what is this";
    char target[13];
    printf("\nwriting 'what is this'\n");
    int bytes = fs_write(d, buffer, 13);
    fs_lseek(d, 0);
    printf("%d bytes written\n", bytes);
    print_disk_struct();

    printf("\nreading 'example' file\n");
    bytes = fs_read(d, target, 13);
    fs_lseek(d, 0);
    printf("%d bytes read\n", bytes);
    print_disk_struct();

    printf("\ntruncating 'example' to 10 bytes\n");
    fs_truncate(0, 10);
    print_disk_struct();

    printf("\nreading truncated 'example' file\n");
    memset(target, 0, 13);
    bytes = fs_read(d, target, 13);
    fs_lseek(d, 0);
    printf("result should be '%.10s': |%s|\n", buffer, target);
    printf("%d bytes read\n", bytes);
    print_disk_struct();

    char chonk[9000];
    for (int i = 0; i < 8999; i++)
    {
        chonk[i] = 'a' + i % 25;
    }
    chonk[8999] = '\0';
    printf("\nwriting a really long file\n");
    bytes = fs_write(d, chonk, 9000);
    fs_lseek(d, 0);
    printf("%d bytes written\n", bytes);
    print_disk_struct();

    printf("\nreading really long file\n");
    memset(target, 0, 13);
    bytes = fs_read(d, target, 12);
    fs_lseek(d, 0);
    printf("%d bytes read\n", bytes);
    printf("result should be '%.12s': |%s|\n", chonk, target);
    print_disk_struct();

    printf("\nreading the whole chonk\n");
    char chonkt[9000];
    memset(chonkt, 0, 9000);
    bytes = fs_read(d, chonkt, 8999);
    fs_lseek(d, 0);
    printf("%d bytes read\n", bytes);
    printf("result should be '%s'", chonk);
    printf(", and it was this|%s|\n", chonkt);
    print_disk_struct();

    printf("\ntruncating that large file\n");
    fs_truncate(d, 10);
    memset(target, 0, 13);
    bytes = fs_read(d, target, 12);
    fs_lseek(d, 0);
    printf("result should be '%.10s': |%s|\n", chonk, target);
    printf("%d bytes read\n", bytes);
    print_disk_struct();

    printf("\ndeleting file\n");
    fs_close(d);
    fs_delete("example");
    print_disk_struct();

    printf("\ncreating max number of files\n");
    char name[2] = { 'a' };
    for (int i = 0; i < MAX_FILES; i++)
    {
        printf("creating file '%s'\n", name);
        fs_create(name);
        name[0]++;
    }
    print_disk_struct();

    printf("\ndeleting all those files\n");
    name[0] = 'a';
    for (int i = 0; i < MAX_FILES; i++)
    {
        printf("deleting file '%s'\n", name);
        fs_delete(name);
        name[0]++;
    }
    print_disk_struct();

    printf("\ncreating & writing a file with max possible size (4096x4096)\n");
    char bigbuf[BLOCK_SIZE];
    memset(bigbuf, 'x', BLOCK_SIZE);
    fs_create("big file");
    int big_file_fildes = fs_open("big file");
    for (int i = 0; i < BLOCK_SIZE; i++)
        fs_write(big_file_fildes, bigbuf, BLOCK_SIZE);
    print_disk_struct();

    printf("\nreading first 100 chars of that giant file\n");
    fs_lseek(big_file_fildes, 0);
    char bigtarget[101];
    memset(bigtarget, 0, 101);
    fs_read(big_file_fildes, bigtarget, 100);
    printf("result: %s\n", bigtarget);

    printf("\ndeleting that giant file\n");
    fs_close(big_file_fildes);
    fs_delete("big file");
    print_disk_struct();


    if (umount_fs(diskname) < 0)
        return 1;

    return 0;
}

int get_diskname(char * name)
{
    int create;

    printf("Create a new disk? (y/n): ");
    create = fgetc(stdin) != 'n';
    fgetc(stdin);

    if (create)
    {
        printf("Name the new disk: ");
    }
    else 
    {
        list_disks();
        printf("Enter the name of disk to load: ");
    }

    fgets(name, DISKNAME_LEN - strlen(DISK_DIR), stdin);
    
    strtok(name, "\n");
    if (name[0] == '\n')
    {
        printf("Invalid disk name!\n");
        exit(1);
    }
    return create;
}

int list_disks()
{
    struct dirent * file;

    DIR * dir = opendir(DISK_DIR);
    if (dir == NULL)
    {
        printf("Couldn't find %s directory\n", DISK_DIR);
        return 1;
    }

    printf("List of available disks:\n");
    while ((file = readdir(dir)) != NULL)
    {
        if (strncmp(file->d_name, ".", 1) != 0 
                && strncmp(file->d_name, "..", 2) != 0)
            printf("  %s\n", file->d_name);
    }

    closedir(dir);
    return 0;
}
