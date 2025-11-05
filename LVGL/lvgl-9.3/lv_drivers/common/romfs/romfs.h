/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019/01/13     Bernard      code cleanup
 */

#ifndef __ROMFS_H__
#define __ROMFS_H__


#include "stdint.h"
#include "string.h"

typedef signed long off_t;


struct romfs_fd
{
    char *path;                  /* Name (below mount point) */
    int ref_count;               /* Descriptor reference count */

    uint32_t flags;              /* Descriptor flags */
    size_t   size;               /* Size in bytes */
    off_t    pos;                /* Current file position */

    void *data;                  /* Specific file system data */
};

struct romfs_dirent
{
    uint32_t      type;  /* dirent type */

    const char       *name; /* dirent name */
    const uint8_t *data; /* file date ptr */
    size_t        size;  /* file size */
};

typedef struct
{
    int fd;     /* directory file */
    char buf[512];
    int num;
    int cur;
} DIR;

struct dirent
{
    uint8_t d_type;           /* The type of the file */
    uint8_t d_namlen;         /* The length of the not including the terminating null file name */
    uint16_t d_reclen;        /* length of this record */
    char d_name[100];   /* The null-terminated file name */
};

void romfs_mount(void *addr);
extern const struct romfs_dirent romfs_root;
int r_open(const char *file, int flags, ...);
int r_close(int fd);
off_t r_lseek(int fd, off_t offset, int whence);
int r_read(int fd, void *buf, size_t len);
int r_ioctl(int fildes, int cmd, ...);
int r_write(int fd, const void *buf, size_t len);
struct dirent *r_readdir(DIR *d);
DIR *r_opendir(const char *name);
int r_closedir(DIR *d);
int r_getsize(int fd);
#endif
