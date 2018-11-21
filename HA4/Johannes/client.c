#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/time.h>

int set(char *key, int key_length, char *value, int value_length) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd); // TODO sockfd no parameter in get()?

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do {
        unsigned char answer_header[key_length + value_length + 6];
        memset(answer_header, 0, key_length + value_length + 6);
        answer_header[0] = 0b00000010;
        answer_header[1] = 0; // no ID ???
        // add key_length to answer
        int length = key_length;
        int LSBMAX = 255;
        int LSB = key_length & LSBMAX;
        int MSB = (length - LSB) >> 8;
        answer_header[2] = MSB;
        answer_header[3] = LSB;
        // add value_length to answer
        length = value_length;
        LSBMAX = 255;
        LSB = value_length & LSBMAX;
        MSB = (length - LSB) >> 8;
        answer_header[4] = MSB;
        answer_header[5] = LSB;
        // add key and value
        memcpy(&answer_header[6], key, key_length);
        memcpy(&answer_header[6 + key_length], value, value_length);

        send(sockfd, answer_header, key_length + value_length + 6, 0);
    } while ((ready = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == 0);
    unsigned char receive_header[1000];
    // recv answer which sockfd in recv? TODO
    ready = recv(sockfd, &receive_header, 1000, 0);
    if (ready == -1) return -1; // error
    if (ready == 1) return 1; // acknowledged

    return 0;
}

int get(char *key, int key_length, char **value) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockdfd);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do {
        send_get((key, keylen));
    } while ((ready = select(sockdfd + 1, &rfds, NULL, NULL, &tv)) == 0);
    {

        return -1;
    }
}

int del(char *key, int key_length) {
}


int main(int argc, char *argv[]) {

    //port and filename
    char *ipdns = argv[1];
    char *port = argv[2];

    //declare needed structures
    struct addrinfo client_info_config;
    struct addrinfo *results;

    //initialize client_info_config
    memset(&client_info_config, 0, sizeof(client_info_config));
    client_info_config.ai_protocol = IPPROTO_TCP;
    client_info_config.ai_family = AF_INET;
    client_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(ipdns, port, &client_info_config, &results) != 0) {
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    int sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(sockfd, results->ai_addr, results->ai_addrlen) == -1) {
        close(sockfd);
        perror("connection error");
    }

    if (argv[4] == "GET") {
        char *val;
        int valuelen = get(argv[5], strlen(argv[5]), &val);
        if (valuelen == -1) {
            perror("GET Error \n");
            exit(1);
        }
        printf("%s", val);
    } else if (argv[4] == "SET") {
        if (set(argv[5], strlen(argv[5]), argv[6], strlen(argv[6])) == -1) {
            perror("SET Error\n");
            exit(1);
        }
    } else if (argv[4] == "DELETE") {
        if (del(argv[5], strlen(argv[5]), argv[6], strlen(argv[6])) == -1) {
            perror("DELETE Error\n");
            exit(1);
        }
    }

    close(sockfd);
    return 0;
}