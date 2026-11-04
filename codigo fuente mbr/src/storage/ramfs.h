#ifndef VOS_RAMFS_H
#define VOS_RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES 8
#define RAMFS_NAME_CAPACITY 16
#define RAMFS_CONTENT_CAPACITY 256

struct ramfs_file {
    char name[RAMFS_NAME_CAPACITY];
    char content[RAMFS_CONTENT_CAPACITY];
    uint16_t length;
};

void ramfs_init(void);
uint32_t ramfs_count(void);
uint32_t ramfs_capacity(void);
const struct ramfs_file *ramfs_get(uint32_t index);
int ramfs_create_note(void);
int ramfs_append(uint32_t index, uint32_t codepoint);
int ramfs_backspace(uint32_t index);

#endif
