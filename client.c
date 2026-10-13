/* client.c - Shared Buffer client (runs on RPi)
 * Forks N children; each connects, sends one integer, prints the reply.
 * Compile: gcc -Wall -o client client.c
 * Run:     ./client N SERVER_IP      e.g. ./client 7 192.168.3.100
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT  8080
#define BUFSZ 256

void child_task(const char *server_ip)
{
    srand(time(NULL) ^ getpid());                   /* different seed per child */
    int value = 1 + rand() % 100;                   /* random integer 1..100 */

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &serv.sin_addr) <= 0) {
        fprintf(stderr, "Bad IP address: %s\n", server_ip);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        exit(1);
    }

    char buf[BUFSZ];
    snprintf(buf, sizeof(buf), "%d", value);
    write(sockfd, buf, strlen(buf));                /* send the integer */

    int n = read(sockfd, buf, sizeof(buf) - 1);     /* receive reply */
    if (n <= 0) { perror("read"); exit(1); }
    buf[n] = '\0';

    int r_value, r_slot, r_sum;
    sscanf(buf, "%d %d %d", &r_value, &r_slot, &r_sum);

    printf("PID: %d | Value sent: %d | Slot index: %d | Buffer sum: %d\n",
           getpid(), value, r_slot, r_sum);

    close(sockfd);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s N SERVER_IP\n", argv[0]);
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) { fprintf(stderr, "N must be > 0\n"); return 1; }

    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); break; }
        if (pid == 0) child_task(argv[2]);          /* child never returns */
    }

    for (int i = 0; i < N; i++) wait(NULL);         /* parent reaps all children */
    return 0;
}
