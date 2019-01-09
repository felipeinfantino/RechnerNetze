#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

int main(int argc, char *argv[]) {

    char *ipdns = argv[1];
    char *port = argv[2];
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    int rv;
    int numbytesS, numbytesR;

    struct timespec start, stop;
    double accum;

    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

    if (argc != 3) {
        fprintf(stderr, "usage: talker hostname message\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(ipdns, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

// loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    char *hello = "Hello Server";
    if ((numbytesS = sendto(sockfd, hello, strlen(hello), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    char buf[512];
    socklen_t addr_len = sizeof(their_addr);
    if ((numbytesS = recvfrom(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("talker: recvfrom");
        exit(1);
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }
    //check if more than 1 passed and correct nanoseconds
    struct timespec tmpTime;
    if ((stop.tv_nsec - start.tv_nsec) < 0) {
        tmpTime.tv_nsec = 1000000000 + stop.tv_nsec - start.tv_nsec; //add 1s in nanoseconds
    } else {
        tmpTime.tv_nsec = stop.tv_nsec - start.tv_nsec;
    }
    accum = tmpTime.tv_nsec / 1000; //convert to ms
    printf("%lf ms\n", accum);

    return 0;
}