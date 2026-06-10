#include "stdlib.h"
#include "romfs.h"
//#include "trace.h"
#include "stdarg.h"
#include "log.h"
#include <stdio.h>

#define O_RDONLY         00
#define O_WRONLY         01
#define O_RDWR           02

#define O_CREAT        0100
#define O_EXCL         0200
#define O_NOCTTY       0400
#define O_TRUNC       01000
#define O_APPEND      02000
#define O_NONBLOCK    04000
#define O_DSYNC      010000
#define O_SYNC     04010000
#define O_RSYNC    04010000
#define O_BINARY    0100000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC  02000000

#define O_ASYNC      020000
#define O_DIRECT     040000
#define O_LARGEFILE 0100000
#define O_NOATIME  01000000
#define O_PATH    010000000
#define O_TMPFILE 020200000
#define O_NDELAY O_NONBLOCK

#define O_SEARCH  O_PATH
#define O_EXEC    O_PATH

#define O_ACCMODE (03|O_SEARCH)
#define S_IFMT               00170000
#define S_IFSOCK             0140000
#define S_IFLNK              0120000
#define S_IFREG              0100000
#define S_IFBLK              0060000
#define S_IFDIR              0040000
#define S_IFCHR              0020000
#define S_IFIFO              0010000
#define S_ISUID              0004000
#define S_ISGID              0002000
#define S_ISVTX              0001000

#define S_ISLNK(m)           (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)           (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)           (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)           (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)           (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)          (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)          (((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU              00700
#define S_IRUSR              00400
#define S_IWUSR              00200
#define S_IXUSR              00100

#define S_IRWXG              00070
#define S_IRGRP              00040
#define S_IWGRP              00020
#define S_IXGRP              00010

#define S_IRWXO              00007
#define S_IROTH              00004
#define S_IWOTH              00002
#define S_IXOTH              00001
#define SEEK_SET 0 /* start of stream (see fseek) */
#define SEEK_CUR 1 /* current position in stream (see fseek) */
#define SEEK_END 2 /* end of stream (see fseek) */

#define ROMFS_DIRENT_FILE   0x00
#define ROMFS_DIRENT_DIR    0x01

#define DT_UNKNOWN           0x00
#define DT_REG               0x01
#define DT_DIR               0x02
#define ESUCCESS     0  /* Operation Success */

#define EPERM        1  /* Operation not permitted */
#define ENOENT       2  /* No such file or directory */
#define ESRCH        3  /* No such process */
#define EINTR        4  /* Interrupted system call */
#define EIO          5  /* I/O error */
#define ENXIO        6  /* No such device or address */
#define E2BIG        7  /* Argument list too long */
#define ENOEXEC      8  /* Exec format error */
#define EBADF        9  /* Bad file number */
#define ECHILD      10  /* No child processes */
#define EAGAIN      11  /* Try again */
#define FS_ENOMEM      12  /* Out of memory */
#define EACCES      13  /* Permission denied */
#define EFAULT      14  /* Bad address */
#define ENOTBLK     15  /* Block device required */
#define EBUSY       16  /* Device or resource busy */
#define EEXIST      17  /* File exists */
#define EXDEV       18  /* Cross-device link */
#define ENODEV      19  /* No such device */
#define ENOTDIR     20  /* Not a directory */
#define EISDIR      21  /* Is a directory */
#define FS_EINVAL      22  /* Invalid argument */
#define ENFILE      23  /* File table overflow */
#define EMFILE      24  /* Too many open files */
#define ENOTTY      25  /* Not a typewriter */
#define ETXTBSY     26  /* Text file busy */
#define EFBIG       27  /* File too large */
#define ENOSPC      28  /* No space left on device */
#define ESPIPE      29  /* Illegal seek */
#define EROFS       30  /* Read-only file system */
#define EMLINK      31  /* Too many links */
#define EPIPE       32  /* Broken pipe */
#define FS_EDOM        33  /* Math argument out of domain of func */
#define FS_ERANGE      34  /* Math result not representable */
#define EVRF        35  /* Verification failed */
#define ENOF        36  /* Not found */


#define ENODATA     61  /* No data available */
#define EOVERFLOW   75  /* Value too large for defined data type */
#define ERESTART    85  /* Interrupted system call should be restarted */
#define ECONNABORTED    103 /* Software caused connection abort */
#define EOPNOTSUPP  95  /* Operation not supported on transport endpoint */
#define ECONNRESET  104 /* Connection reset by peer */
#define ENOBUFS     105 /* No buffer space available */
#define ESHUTDOWN   108 /* Cannot send after transport endpoint shutdown */
#define ETIMEDOUT   110 /* Connection timed out */
#define EINPROGRESS 115 /* Operation now in progress */
#define ENOTSUPP        524     /* Operation is not supported */

// default romfs address
static void *romfs_addr = (void *)0x703000;

void romfs_mount(void *addr)
{
    romfs_addr = addr;
}


int check_dirent(struct romfs_dirent *dirent)
{
    if ((dirent->type != ROMFS_DIRENT_FILE && dirent->type != ROMFS_DIRENT_DIR)
        || dirent->size == ~0)
    {
        return -1;
    }
    return 0;
}

struct romfs_dirent *romfs_lookup(struct romfs_dirent *root_dirent, const char *path, size_t *size)
{
#ifdef DEBUG
    printf("romfs_lookup start %s\n", path);
#endif
    size_t index, found;
    const char *subpath, *subpath_end;
    struct romfs_dirent *dirent;
    size_t dirent_size;
#ifdef DEBUG
    printf("romfs_lookup check root dirent type %ld size %d\n", root_dirent->type, root_dirent->size);
#endif
    /* Check the root_dirent. */
    if (check_dirent(root_dirent) != 0)
    {
        printf("romfs_lookup check root dirent is null\n");
        return NULL;
    }

    if (path)
    {
        if (path[0] == '/' && path[1] == '\0')
        {
            *size = root_dirent->size;
            return root_dirent;
        }
    }

    /* goto root directory entries */
    dirent = (struct romfs_dirent *)root_dirent->data;
    dirent_size = root_dirent->size;

    /* get the end position of this subpath */
    subpath_end = path;
    /* skip /// */
    if (subpath_end)
    {
        while (*subpath_end == '/')
        {
            subpath_end ++;
        }
        subpath = subpath_end;
        while ((*subpath_end != '/') && *subpath_end)
        {
            subpath_end ++;
        }
    }

    while (dirent != NULL)
    {
        found = 0;

        /* search in folder */
        for (index = 0; index < dirent_size; index ++)
        {
#ifdef DEBUG
            printf("dirent[index].name %s , subpath %s\n", dirent[index].name, subpath);
#endif
            if (check_dirent(&dirent[index]) != 0)
            {
                printf("romfs_lookup check folder dirent is null\n");
                return NULL;
            }
            if (subpath_end && subpath && (subpath_end - subpath) >= 0)
            {
                if (strlen(dirent[index].name) == (subpath_end - subpath) &&
                    strncmp(dirent[index].name, subpath, (subpath_end - subpath)) == 0)
                {
                    dirent_size = dirent[index].size;

                    /* skip /// */
                    if (subpath_end)
                    {
                        while (*subpath_end == '/')
                        {
                            subpath_end ++;
                            if (!subpath_end)
                            {
                                break;
                            }
                        }
                        subpath = subpath_end;
                        while ((*subpath_end != '/') && *subpath_end)
                        {
                            subpath_end ++;
                        }
                    }
                    char sp = 0;
                    if (subpath != NULL)
                    {
                        sp = *subpath;
                    }
                    if (!(sp))
                    {
                        *size = dirent_size;
                        return &dirent[index];
                    }

                    if (dirent[index].type == ROMFS_DIRENT_DIR)
                    {
                        /* enter directory */
                        dirent = (struct romfs_dirent *)dirent[index].data;
                        found = 1;
                        break;
                    }
                    else
                    {
                        /* return file dirent */
                        //if (subpath != NULL)
                        {
                            break;    /* not the end of path */
                        }

                        //return &dirent[index];
                    }
                }
            }
        }

        if (!found)
        {
            break;    /* not found */
        }
    }

    /* not found */
    printf("romfs_lookup not found\n");
    return NULL;
}

int romfs_read(struct romfs_fd *file, void *buf, size_t count)
{
    size_t length;
    struct romfs_dirent *dirent;

    dirent = (struct romfs_dirent *)file->data;

    if (check_dirent(dirent) != 0)
    {
        return -EIO;
    }

    if (count < file->size - file->pos)
    {
        length = count;
    }
    else
    {
        length = file->size - file->pos;
    }

    if (length > 0)
    {
        memcpy(buf, &(dirent->data[file->pos]), length);
    }

    /* update file current position */
    file->pos += length;

    return length;
}

int romfs_lseek(struct romfs_fd *file, off_t offset)
{
    if (offset <= file->size)
    {
        file->pos = offset;
        return file->pos;
    }

    return -EIO;
}

int romfs_close(struct romfs_fd *file)
{
    file->data = NULL;
    return 0;
}

int romfs_open(struct romfs_fd *file)
{
#ifdef DEBUG
    printf("romfs_open\n");
#endif
    size_t size;
    struct romfs_dirent *dirent;
    struct romfs_dirent *root_dirent;

    if (file)
    {
        root_dirent = file->data;

        if (check_dirent(root_dirent) != 0)
        {
            printf("romfs_open EIO fail\n");
            return -EIO;
        }

        if (file->flags & (O_CREAT | O_WRONLY | O_APPEND | O_TRUNC | O_RDWR))
        {
            printf("romfs_open EINVAL fail\n");
            return -FS_EINVAL;
        }

        dirent = romfs_lookup(root_dirent, file->path, &size);
        if (dirent == NULL)
        {
            printf("romfs_open ENOENT fail\n");
            return -ENOENT;
        }

        /* entry is a directory file type */
        if (dirent->type == ROMFS_DIRENT_DIR)
        {
            if (!(file->flags & O_DIRECTORY))
            {
                printf("romfs_open ENOENT fail 2\n");
                return -ENOENT;
            }
        }
        else
        {
            /* entry is a file, but open it as a directory */
            if (file->flags & O_DIRECTORY)
            {
                printf("romfs_open ENOENT fail 3\n");
                return -ENOENT;
            }
        }

        file->data = dirent;
        file->size = size;
        file->pos = 0;
    }

    return 0;
}

int r_open(const char *file, int flags, ...)
{
#ifdef DEBUG
    printf("romfs r_open %s flags %d\n", file, flags);
#endif
    int result;
    struct romfs_fd *fd;

    /* allocate a fd */
    fd = malloc(sizeof(struct romfs_fd));
    if (fd)
    {
        fd->flags = flags;
        fd->size  = 0;
        fd->pos   = 0;
        fd->data  = (void *)romfs_addr;
        fd->path = (char *)file;
    }

    result = romfs_open(fd);
    if (result < 0)
    {
        free(fd);
        return -1;
    }

    int f = (int)fd;

    return f;
}

int r_close(int fd)
{
    int result;
    struct romfs_fd *d = (struct romfs_fd *)fd;

    if (d == NULL)
    {
        return -1;
    }

    result = romfs_close(d);

    if (result < 0)
    {
        return -1;
    }

    free(d);
    d = NULL;

    return 0;
}

int r_read(int fd, void *buf, size_t len)
{
    int result;
    struct romfs_fd *d = (struct romfs_fd *)fd;

    /* get the fd */
    if (d == NULL)
    {
        return -1;
    }

    result = romfs_read(d, buf, len);
    if (result < 0)
    {
        return -1;
    }

    /* release the ref-count of fd */

    return result;
}

off_t r_lseek(int fd, off_t offset, int whence)
{
    int result;
    struct romfs_fd *d = (struct romfs_fd *)fd;

    if (d == NULL)
    {
        return -1;
    }

    switch (whence)
    {
    case SEEK_SET:
        break;

    case SEEK_CUR:
        offset += d->pos;
        break;

    case SEEK_END:
        offset += d->size;
        break;

    default:

        return -1;
    }

    if (offset < 0)
    {
        return -1;
    }
    result = romfs_lseek(d, offset);
    if (result < 0)
    {
        return -1;
    }

    /* release the ref-count of fd */
    return offset;
}
/**
 * this function is a POSIX compliant version, which will close a directory
 * stream.
 *
 * @param d the directory stream.
 *
 * @return 0 on successful, -1 on failed.
 */
int r_closedir(DIR *d)
{
    if (d->fd == 0)
    {

        return -1;
    }
    r_close(d->fd);
    free(d);
    return 0;
}
/**
 * this function is a POSIX compliant version, which will open a directory.
 *
 * @param name the path name to be open.
 *
 * @return the DIR pointer of directory, NULL on open directory failed.
 */
DIR *r_opendir(const char *name)
{
#ifdef DEBUG
    printf("romfs opendir %s\n", name);
#endif
    int fd;
    DIR *t;

    t = NULL;

    fd = r_open(name, O_RDONLY | O_DIRECTORY);
    if (fd >= 0)
    {
        /* open successfully */
        t = (DIR *) malloc(sizeof(DIR));
        if (t == NULL)
        {
            r_close(fd);
        }
        else
        {
            memset(t, 0, sizeof(DIR));
            t->fd = fd;
        }

        return t;
    }

    /* open failed */

    return NULL;
}
static int dfs_romfs_getdents(struct romfs_fd *file, struct dirent *dirp, uint32_t count)
{
    size_t index;
    const char *name;
    struct dirent *d;
    struct romfs_dirent *dirent, *sub_dirent;
    dirent = (struct romfs_dirent *)file->data;
    if (check_dirent(dirent) != 0)
    {
        return -1;
    }


    /* enter directory */
    dirent = (struct romfs_dirent *)dirent->data;

    /* make integer count */
    count = (count / sizeof(struct dirent));
    if (count == 0)
    {
        return -2;
    }

    index = 0;
    for (index = 0; index < count && file->pos < file->size; index ++)
    {
        d = dirp + index;

        sub_dirent = &dirent[file->pos];
        name = sub_dirent->name;

        /* fill dirent */
        if (sub_dirent->type == ROMFS_DIRENT_DIR)
        {
            d->d_type = DT_DIR;
        }
        else
        {
            d->d_type = DT_REG;
        }

        d->d_namlen = strlen(name);
        d->d_reclen = (uint16_t)sizeof(struct dirent);
        strncpy(d->d_name, name, strlen(name) + 1);

        /* move to next position */
        ++ file->pos;
    }

    return index * sizeof(struct dirent);
}
/**
 * @ingroup Fd
 *
 * This function will return a file descriptor structure according to file
 * descriptor.
 *
 * @return NULL on on this file descriptor or the file descriptor structure
 * pointer.
 */
static struct romfs_fd *fd_get(int fd)
{
    return (void *)fd;
}
static int dfs_romfs_ioctl(struct romfs_fd *file, int cmd, void *args)
{
    switch (cmd)
    {
    case 0:
        {
            struct romfs_dirent *dirent;

            dirent = (struct romfs_dirent *)file->data;

            if (check_dirent(dirent) != 0)
            {
                return -EIO;
            }

            return (int) & (dirent->data[file->pos]);

        }
    }
    return -EIO;
}
/**
 * this function is a POSIX compliant version, which will return a pointer
 * to a dirent structure representing the next directory entry in the
 * directory stream.
 *
 * @param d the directory stream pointer.
 *
 * @return the next directory entry, NULL on the end of directory or failed.
 */
struct dirent *r_readdir(DIR *d)
{
    int result;
    struct romfs_fd *fd;

    fd = fd_get(d->fd);
    if (fd == NULL)
    {
		RTK_LOGS(NOTAG, RTK_LOG_ERROR, "fd == NULL");
        return NULL;
    }

    if (d->num)
    {
        struct dirent *dirent_ptr;
        dirent_ptr = (struct dirent *)&d->buf[d->cur];
        d->cur += dirent_ptr->d_reclen;
    }

    if (!d->num || d->cur >= d->num)
    {
        /* get a new entry */
        result = dfs_romfs_getdents(fd,
                                    (struct dirent *)d->buf,
                                    sizeof(d->buf) - 1);
        if (result <= 0)
        {


            return NULL;
        }

        d->num = result;
        d->cur = 0; /* current entry index */
    }



    return (struct dirent *)(d->buf + d->cur);
}
/**
 * this function is a POSIX compliant version, which will write specified data
 * buffer length for an open file descriptor.
 *
 * @param fd the file descriptor
 * @param buf the data buffer to be written.
 * @param len the data buffer length.
 *
 * @return the actual written data buffer length.
 */
int r_write(int fd, const void *buf, size_t len)
{
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}
int fcntl(int fildes, int cmd, ...)
{
    int ret = -1;
    struct romfs_fd *d;

    /* get the fd */
    d = fd_get(fildes);
    if (d)
    {
        void *arg;
        va_list ap;

        va_start(ap, cmd);
        arg = va_arg(ap, void *);
        va_end(ap);

        ret = dfs_romfs_ioctl(d, cmd, arg);
    }
    else { ret = -EBADF; }

    if (ret < 0)
    {
        ret = -1;
    }

    return ret;
}
/**
 * this function is a POSIX compliant version, which shall perform a variety of
 * control functions on devices.
 *
 * @param fildes the file description
 * @param cmd the specified command
 * @param data represents the additional information that is needed by this
 * specific device to perform the requested function.
 *
 * @return 0 on successful completion. Otherwise, -1 shall be returned and errno
 * set to indicate the error.
 */
int r_ioctl(int fildes, int cmd, ...)
{
    void *arg;
    va_list ap;

    va_start(ap, cmd);
    arg = va_arg(ap, void *);
    va_end(ap);

    /* we use fcntl for this API. */
    return fcntl(fildes, cmd, arg);
}

int r_getsize(int fd) {
    struct romfs_fd *r = fd_get(fd);
    return r->size;
}