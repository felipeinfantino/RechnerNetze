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

int main(int argc, char *argv[]) {

    char *ipdns = argv[1];
    char *port = argv[2];

    struct addrinfo *servinfo;
    struct addrinfo hints;

    int csocket = socket(AF_UNSPEC, SOCK_STREAM, 0); //socket()

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    int status = getaddrinfo(ipdns, port, &hints, &servinfo);
    if(status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    int s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    int connection = connect(s, servinfo->ai_addr, servinfo->ai_addrlen);
    if(connection == -1){
        perror("connection error");
        exit(1);
    }

    char response[1000];
    int receive = recv(s, &response, sizeof(response), 0);
    if(receive == -1){
        perror("receive error");
        exit(1);
    }

    printf("%s", response);

    close(s);
    freeaddrinfo(servinfo); // free the linked-list
    return 0;
}