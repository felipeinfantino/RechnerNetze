#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

int main(int argc, char *argv[]) {

    char *ipdns = argv[1];
    char *port = argv[2];
    struct addrinfo *servinfo, *nextsocket;
    struct addrinfo hints;

    struct timespec start, stop;
    double accum;

    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }

    if (argc != 3) { // test for right usage
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    int status = getaddrinfo(ipdns, port, &hints, &servinfo); // get Server info and safe in servinfo
    if(status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    int sockfd;
    // go through all results to find one to connect
    for(nextsocket = servinfo; nextsocket != NULL; nextsocket = nextsocket->ai_next) {
        if ((sockfd = socket(nextsocket->ai_family, nextsocket->ai_socktype, nextsocket->ai_protocol)) == -1) {
            perror("socket error");
            continue;
        }
        if (connect(sockfd, nextsocket->ai_addr, nextsocket->ai_addrlen) == -1) {
            close(sockfd);
            perror("connection error");
            continue;
        }
        break;
    }

    char response[512];
    int receive;
    if ((receive = recv(sockfd, &response, sizeof(response), 0)) == -1){
        perror("talker: recvfrom");
        exit(1);
    }

    printf("%s",response );
    close(sockfd);   //  close socket
    freeaddrinfo(servinfo); // free the linked-list

    if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }
    //check if more than 1 passed and correct nanoseconds
    struct timespec tmpTime;
    if ((stop.tv_nsec - start.tv_nsec) < 0) {
        tmpTime.tv_nsec = 1000000000 + stop.tv_nsec - start.tv_nsec; //add 1s in nanoseconds
    } else {
        tmpTime.tv_nsec = stop.tv_nsec - start.tv_nsec;
    }
    accum = tmpTime.tv_nsec / 1000000; //convert to ms
    printf("\n%lf ms\n", accum);
    
    return 0;
}