#include <cos_types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


void die(const char *errmsg) {
    printf("%s\n", errmsg);
    exit(1);
}
 
int main()
{
 
    char str[100];
    int listen_fd, comm_fd;
 
    struct sockaddr_in servaddr;
 
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) die("socket failed\n");
 
    bzero(&servaddr, sizeof(servaddr));
 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = 22000;
 
    if (bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) die("bind fail\n");
 
    if (listen(listen_fd, 10) == -1) die("listen fail\n");
 
    printf("Waiting for a connection...\n");

    comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
    if (comm_fd == -1) die("accept fail\n");
 
    printf("Connected to Client\n");

    while(1)
    {
        bzero(str, 100);
        read(comm_fd,str,100);
        printf("Echoing back - %s\n",str);
        write(comm_fd, str, strlen(str)+1);
    }
}

