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
    printf("result should be '%s': |%s|\n", buffer, target);
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
    int a =printf("result should be '%s'", chonk);
    int b = printf(", and it was this|%s|\n", chonkt);
    printf("printed %d %d\n", a, b);
    print_disk_struct();


    fs_close(0);

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
