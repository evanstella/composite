#include <cos_types.h>

#define SYSCALLS_NUM 500

typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);

/* we need some dynamic memory allocation. */
extern void *malloc(size_t sz);

/* 
 * File Descriptor capability for composite.
 * The goal is to allow higher level posix api implementations
 * to decide how they want to use file descriptors that are
 * passed throught the posix layer, so this is meant to be
 * as generic and extensible as possible.
 */
typedef ssize_t (*cos_posix_write_fn_t)(const char *buf, size_t count);
typedef ssize_t (*cos_posix_read_fn_t)(char *buf, size_t count);

/* 
 * files can provide a setup function to be called when a thread is created.
 * example is network sockets, as each thread in composite needs to bind
 * to the active port when it is created
 */
typedef void (*cos_posix_new_thd_setup_fn_t)(int fd);

#define POSIX_NUM_FD 1024

struct cos_posix_file_generic {
	cos_posix_new_thd_setup_fn_t setup_fn;
	cos_posix_write_fn_t         write;
    cos_posix_read_fn_t          read;
    /* stat, seek, sockoptions ioctls m/unmap */
    unsigned char        __data[128]; /* non-generic implementations define what goes here */
};

int                            cos_posix_fd_alloc();
struct cos_posix_file_generic *cos_posix_fd_get(int fd);
void                           cos_posix_new_thread_call_setup(void);

void libc_syscall_override(cos_syscall_t fn, int syscall_num);
