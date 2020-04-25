#include "filesystem.h"

#define MAX_OPEN_FILES 32
#define DESCRIPTOR_UNUSED -1

/* Descriptor -- file descriptor for an open file
 * Don't store this struct on disk! Generate it on the fly
 * descriptor: file descriptor (0-31)
 * ptr: where the next read/write should occur in this file
 */
typedef struct {
    int descriptor;
    Attribute * attr;
    char * ptr;
} Descriptor;

Descriptor descriptors[MAX_OPEN_FILES];
