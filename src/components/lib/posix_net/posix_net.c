#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <cos_component.h>
#include <posix.h>
#include <netmgr.h>
#include <netshmem.h>



struct cos_posix_file_socket {
	cos_posix_write_fn_t write;
	cos_posix_read_fn_t  read;
	int                  domain, type, protocol;
	socklen_t            addr_len;
	struct sockaddr      addr;
};

static ssize_t 
tcp_socket_read(char *buf, size_t count)
{
	char *data;
	u16_t data_offset, data_len;

	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *rx_obj;

	objid  = netmgr_tcp_shmem_read(&data_offset, &data_len);
	rx_obj = shm_bm_take_net_pkt_buf(netshmem_get_shm(), objid);
	
	data = rx_obj->data + data_offset;

	if (count > data_len) data_len = count;

	memcpy(buf, data, data_len);
	shm_bm_free_net_pkt_buf(rx_obj);

	return data_len;
}

static ssize_t 
tcp_socket_write(const char *buf, size_t count)
{
	shm_bm_objid_t           objid = 0;
	struct netshmem_pkt_buf *tx_obj;

	tx_obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
	memcpy(netshmem_get_data_buf(tx_obj), buf, count);

	netmgr_tcp_shmem_write(objid, netshmem_get_data_offset(), count);
	shm_bm_free_net_pkt_buf(tx_obj);

	return count;
}

int
cos_socket(int domain, int type, int protocol)
{
	int fd = -1;
	struct cos_posix_file_socket *sock;

	/* A little limited in capabilities... */
	if (domain != AF_INET) return -1;

	fd = cos_posix_fd_alloc();
	if (fd == -1) return -1;

	sock = (struct cos_posix_file_socket *)cos_posix_fd_get(fd);
	if (sock == NULL) return -1;

	if (type & SOCK_STREAM) {
		sock->type = SOCK_STREAM;
	} else {
		return -1;
	}

	sock->domain   = domain;
	sock->protocol = protocol;

	return fd;
}

int
cos_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	int ret;
	struct cos_posix_file_socket *sock;
	
	sock = (struct cos_posix_file_socket *)cos_posix_fd_get(fd);
	if (sock == NULL) return -1;
	/* TODO: this should fail if we try to rebind a socket */

	switch (sock->type)
	{
		case SOCK_STREAM : {
			struct sockaddr_in *inaddr = (struct sockaddr_in *)addr;

			ret = netmgr_tcp_bind(inaddr->sin_addr.s_addr, htons(inaddr->sin_port));
			if (ret != NETMGR_OK) return -1;

			break;
		}
		default: {
			return -1;
		}
	}

	sock->addr_len = len;
	memcpy(&sock->addr, addr, len);

	return 0;
}

int 
cos_listen(int fd, int backlog)
{
	int ret;
	struct cos_posix_file_socket *sock;

	sock = (struct cos_posix_file_socket *)cos_posix_fd_get(fd);
	if (sock == NULL) return -1;

	switch (sock->type)
	{
		case SOCK_STREAM : {
			ret = netmgr_tcp_listen((u8_t)backlog);
			if (ret != NETMGR_OK) return -1;

			break;
		}
		default: {
			return -1;
		}
	}

	return 0;
}

int
cos_accept(int fd, struct sockaddr *sockaddr_ret, socklen_t *len_ret)
{
	int ret, conn_fd;
	struct cos_posix_file_socket *sock, *conn;
	struct conn_addr client_addr;

	sock = (struct cos_posix_file_socket *)cos_posix_fd_get(fd);
	if (sock == NULL) return -1;

	/* reserve a fd for the connection */
	conn_fd = cos_posix_fd_alloc();
	if (conn_fd == -1) return -1; /* TODO dealloc */
	conn = (struct cos_posix_file_socket *)cos_posix_fd_get(conn_fd);

	switch (sock->type)
	{
		case SOCK_STREAM : {
			ret = netmgr_tcp_accept(&client_addr);
			if (ret != NETMGR_OK) return -1;

			conn->read     = tcp_socket_read;
			conn->write    = tcp_socket_write;
			conn->type     = sock->type;
			conn->domain   = sock->domain;
			conn->protocol = sock->protocol;
			memcpy(&conn->addr, &sock->addr, sock->addr_len);

			if (sockaddr_ret && len_ret) {
				struct sockaddr_in *addr_ret = (struct sockaddr_in *)sockaddr_ret;
				addr_ret->sin_family = AF_INET;
				addr_ret->sin_addr.s_addr = client_addr.ip;
				addr_ret->sin_port = client_addr.port;
				*len_ret = sizeof(struct sockaddr_in);
			}

			break;
		}
		default: {
			return -1; /* TODO dealloc */
		}
	}

	return conn_fd;
}

int 
cos_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
	struct cos_posix_file_socket *sock = (struct cos_posix_file_socket *)cos_posix_fd_get(sockfd);
	if (sock == NULL || sock->read == NULL) return 0;

	if (flags != 0) {
		printc("recvfrom: flags ignored");
	}

	switch (sock->type)
	{
		case SOCK_STREAM : {
			if (src_addr && addrlen) {
				memcpy(src_addr, &sock->addr, sock->addr_len);
				*addrlen = sock->addr_len;
			}

			return tcp_socket_read(buf, len);
		}
		default: {
			return 0;
		}
	} 
}

int
cos_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	return 0;
}

void
libc_posixnet_initialization_handler()
{
	libc_syscall_override((cos_syscall_t)(void *)cos_socket,     __NR_socket);
	libc_syscall_override((cos_syscall_t)(void *)cos_bind,       __NR_bind);
	libc_syscall_override((cos_syscall_t)(void *)cos_listen,     __NR_listen);
	libc_syscall_override((cos_syscall_t)(void *)cos_accept,     __NR_accept);
	libc_syscall_override((cos_syscall_t)(void *)cos_accept,     __NR_accept4);
	libc_syscall_override((cos_syscall_t)(void *)cos_setsockopt, __NR_setsockopt);
	libc_syscall_override((cos_syscall_t)(void *)cos_recvfrom,   __NR_recvfrom);
	
	/* create current component's shmem for netmgr and map it to netmgr */
	netshmem_create();
	netmgr_shmem_map(netshmem_get_shm_id());
}
