/* server.c - Shared Buffer server (runs on PC)
 * One thread per client; shared circular buffer (size 5), write index and sum,
 * protected by a mutex.
 * Compile: gcc -Wall -o server server.c -lpthread
 * Run:     ./server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT      8080
#define BUF_SIZE  5
#define BUFSZ     256

int buffer[BUF_SIZE] = {0};            /* shared circular buffer */
int write_index = 0;                   /* next slot to write */
int sum = 0;                           /* sum of values currently in buffer */
pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    int connfd = *(int *)arg;
    free(arg);

    char msg[BUFSZ], reply[BUFSZ];
    int n = read(connfd, msg, sizeof(msg) - 1);
    if (n <= 0) { close(connfd); return NULL; }
    msg[n] = '\0';

    int value = atoi(msg);
    int slot, cur_sum;

    pthread_mutex_lock(&buf_mutex);                 /* ---- enter critical section ---- */
    slot = write_index;
    sum -= buffer[slot];                            /* remove the value being overwritten */
    buffer[slot] = value;                           /* insert new value */
    sum += value;                                   /* add new value to sum */
    write_index = (write_index + 1) % BUF_SIZE;     /* advance, wrap around */
    cur_sum = sum;
    pthread_mutex_unlock(&buf_mutex);               /* ---- leave critical section ---- */

    printf("Received %d -> slot %d, buffer sum = %d\n", value, slot, cur_sum);
    fflush(stdout);

    snprintf(reply, sizeof(reply), "%d %d %d", value, slot, cur_sum);
    write(connfd, reply, strlen(reply));

    close(connfd);
    return NULL;
}

int main(void)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(listenfd, 10) < 0) { perror("listen"); exit(1); }

    printf("Buffer server listening on port %d (buffer size %d)\n", PORT, BUF_SIZE);

    while (1) {
        int *connfd = malloc(sizeof(int));
        *connfd = accept(listenfd, NULL, NULL);
        if (*connfd < 0) { perror("accept"); free(connfd); continue; }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, connfd) != 0) {
            perror("pthread_create");
            close(*connfd);
            free(connfd);
            continue;
        }
        pthread_detach(tid);
    }
    return 0;
}
