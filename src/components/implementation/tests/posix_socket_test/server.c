#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>


void die(const char *errmsg) {
    printf("%s\n", errmsg); /* added to trello: perror no-worky? */
    exit(1);
}
 
#define NUM_THREAD 1

struct thread_arg {
    int id;
};

int sockfd;

void *thread_fn(void *thread_arg)
{
    //Get the socket descriptor
    struct thread_arg *args = (struct thread_arg *)thread_arg;
    size_t sz;
    char buf[200];
    struct sockaddr_in client;
    socklen_t len = sizeof client;

    
    while(1)
    {
        sz = recvfrom(sockfd, &buf, 200, 0, (struct sockaddr *)&client, &len);
        //printf("Thread %d handling request... message: %s\n", args->id, buf);

        sendto(sockfd, &buf, sz, 0, (struct sockaddr *)&client, len);

		//clear the message buffer
		memset(&buf, 0, 200);
    }
     
         
    return 0;
}
 
int main(int argc , char *argv[])
{
    struct sockaddr_in server;
     
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        die("Could not create socket");
    }
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);
     
    if(bind(sockfd,(struct sockaddr *)&server , sizeof(server)) < 0) {
        die("bind failed. Error");
    }
     
	pthread_t threads[NUM_THREAD];
    struct thread_arg args[NUM_THREAD];
    int id = 0;
	
    for (int i = 0; i < NUM_THREAD; i++)
    {
        args[i].id = id++;
        if(pthread_create(&threads[i], NULL, thread_fn, (void*) &args[i]) < 0) {
            die("could not create thread");
        }
    }
     
    /* just wait... */
    while(1) sleep(200);

     
    return 0;
}
