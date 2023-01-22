#include <cos_types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>


void die(const char *errmsg) {
    printf("%s %d\n", errmsg, errno); /* added to trello: perror no-worky? */
    exit(1);
}

void *thread(void *arg) {
    char *ret;
    printf("In new Thread!\n");
    printf("thread() entered with argument '%s'\n", arg);
    if ((ret = (char *)malloc(20)) == NULL) {
        die("malloc() error");
        exit(2);
    }
    strcpy(ret, "This is a test");
    pthread_exit(ret);
}
 
int main()
{
    pthread_t thid;
    void *ret;
    int r;
    printf("start main\n");

    if ((r = pthread_create(&thid, NULL, thread, "thread 1")) != 0) {
        die("pthread_create() error");
    }

    if (pthread_join(thid, &ret) != 0) {
        die("pthread_create() error");
    }

    printf("Thread said: %s\n", ret);

}

