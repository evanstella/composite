#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <posix.h>
#include <ps_list.h>
#include <memmgr.h>
#include <contigmem.h>

static struct ps_lock stdout_lock;

ssize_t
write_bytes_to_stdout(const char *buf, size_t count)
{
	size_t i;

	ps_lock_take(&stdout_lock);
	for (i = 0; i < count; i++) printc("%c", buf[i]);
	ps_lock_release(&stdout_lock);

	return count;
}

ssize_t
read_bytes_from_stdin(char *buf, size_t count)
{
	assert(0);
	return 0;
}

ssize_t
cos_write(int fd, const char *buf, size_t count)
{
	struct cos_posix_file_generic *f = cos_posix_fd_get(fd);

	if (f == NULL || f->write == NULL) return 0;

	return f->write(buf, count);
}

ssize_t
cos_read(int fd, char *buf, size_t count)
{
	struct cos_posix_file_generic *f = cos_posix_fd_get(fd);
	if (f == NULL || f->read == NULL) return 0;

	return f->read(buf, count);
}

ssize_t
cos_writev(int fd, const struct iovec *iov, int iovcnt)
{
	int                            i;
	ssize_t                        ret = 0;
	struct cos_posix_file_generic *f = cos_posix_fd_get(fd);

	if (f == NULL || f->write == NULL) return 0;

	for (i=0; i<iovcnt; i++) {
		ret += f->write((const void *)iov[i].iov_base, iov[i].iov_len);
	}

	return ret;
}

long
cos_ioctl(int fd, int request, void *data)
{
	/* musl libc does some ioctls to stdout, so just allow these to silently go through */
	if (fd == 1 || fd == 2) return 0;

	printc("ioctl on fd(%d) not implemented\n", fd);
	assert(0);
	return 0;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret = 0;

	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	addr = (void *)contigmem_alloc((length / PAGE_SIZE));
	if (!addr){
		ret = (void *) -1;
	} else {
		ret = addr;
	}

	if (ret == (void *)-1) {  /* return value comes from man page */
		printc("mmap() failed!\n");
		/* This is a best guess about what went wrong */
		errno = ENOMEM;
	}
	return ret;
}

int
cos_munmap(void *start, size_t length)
{
	printc("munmap not implemented\n");
	errno = ENOSYS;
	return -1;
}

int
cos_madvise(void *start, size_t length, int advice)
{
	/* We don't do anything with the advice from madvise, but that isn't really a problem */
	return 0;
}

void *
cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
	printc("mremap not implemented\n");
	errno = ENOSYS;
	return (void*) -1;
}

int
cos_mprotect(void *addr, size_t len, int prot)
{
	/* Musl uses this at thread create time */
	return ENOSYS;
}

int 
cos_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	unsigned long i;

	for (i = 0; i < nfds; i++) {
		fds[i].revents &= POLLNVAL;
	}

	return 0;
}

char *
cos_getcwd(char *buf, size_t size)
{
	if (buf != NULL) {
		strcpy(buf, "/");
		return buf;
	}

	return NULL;
}

static int
console_init(cos_posix_write_fn_t w, cos_posix_read_fn_t r)
{
	/* dont need any more functionality other than read or write for now */
	struct cos_posix_file_generic *f; 
	int fd;

	fd = cos_posix_fd_alloc();
	if (fd == -1) return -1;

	f = cos_posix_fd_get(fd);

	f->read  = r;
	f->write = w;

	return fd;
}

void
libc_posixcap_initialization_handler()
{
	ps_lock_init(&stdout_lock);
	libc_syscall_override((cos_syscall_t)(void*)cos_write, __NR_write);
	libc_syscall_override((cos_syscall_t)(void*)cos_read, __NR_read);
	libc_syscall_override((cos_syscall_t)(void*)cos_writev, __NR_writev);
	libc_syscall_override((cos_syscall_t)(void*)cos_ioctl, __NR_ioctl);
	libc_syscall_override((cos_syscall_t)(void*)cos_munmap, __NR_munmap);
	libc_syscall_override((cos_syscall_t)(void*)cos_madvise, __NR_madvise);
	libc_syscall_override((cos_syscall_t)(void*)cos_mremap, __NR_mremap);
	libc_syscall_override((cos_syscall_t)(void*)cos_mprotect, __NR_mprotect);
	libc_syscall_override((cos_syscall_t)(void*)cos_mmap, __NR_mmap);
	libc_syscall_override((cos_syscall_t)(void*)cos_poll, __NR_poll);
	libc_syscall_override((cos_syscall_t)(void*)cos_getcwd, __NR_getcwd);

	/* stdin, stdout, stderr */
	assert(console_init(NULL, read_bytes_from_stdin) == STDIN_FILENO);
	assert(console_init(write_bytes_to_stdout, NULL) == STDOUT_FILENO);
	assert(console_init(write_bytes_to_stdout, NULL) == STDERR_FILENO);
}
