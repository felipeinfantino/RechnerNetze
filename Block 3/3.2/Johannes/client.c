#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

int sockfd = 0;

int set(char *key, int key_length, char **value, int value_length)
{

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do
    {
        unsigned char answer_header[key_length + value_length + 6];
        memset(answer_header, 0, key_length + value_length + 6);
        answer_header[0] = 0b00000010;
        answer_header[1] = 0; // no ID
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
        memcpy(&answer_header[6 + key_length], *value, value_length);
        send(sockfd, answer_header, key_length + value_length + 6, 0);
    } while ((ready = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == 0);
    unsigned char receive_header[1000];
    ready = recv(sockfd, &receive_header, 1000, 0);
    if (ready == -1)
        return -1; // error
    if (ready == 1)
        return 1; // acknowledged

    return 0;
}

int get(char *key, int key_length, char **value)
{

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do
    {
        unsigned char answer_header[key_length + 6];
        memset(answer_header, 0, key_length + 6);
        answer_header[0] = 0b00000100;
        answer_header[1] = 0; // no ID
        // add key_length to answer
        int length = key_length;
        int LSBMAX = 255;
        int LSB = key_length & LSBMAX;
        int MSB = (length - LSB) >> 8;
        answer_header[2] = MSB;
        answer_header[3] = LSB;
        // add key
        memcpy(&answer_header[6], key, key_length);

        send(sockfd, answer_header, key_length + 6, 0);
    } while ((ready = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == 0);

    int buffer_size = 7 + key_length + 65536; //header + key_length + maximal value length +1 for '\0'
    unsigned char receive_header[buffer_size];
    ready = recv(sockfd, &receive_header, buffer_size, 0);
    key_length = (receive_header[2] << 8) + receive_header[3];       //process the key length
    int value_length = (receive_header[4] << 8) + receive_header[5]; //process the value length

    *value = calloc(value_length, sizeof(char));
    memcpy(&(*value[0]), &receive_header[6 + key_length], value_length);
    if (value_length > 0)
        value[value_length] = '\0'; //append \0 to value

    if (ready == -1)
        return -1; // error
    if (ready == 1)
        return value_length; // acknowledged

    return 0;
}

int del(char *key, int key_length)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do
    {
        unsigned char answer_header[key_length + 6];
        memset(answer_header, 0, key_length + 6);
        answer_header[0] = 0b00000001;
        answer_header[1] = 0; // set transaction id as 0
        // add key_length to answer
        int length = key_length;
        int LSBMAX = 255;
        int LSB = key_length & LSBMAX;
        int MSB = (length - LSB) >> 8;
        answer_header[2] = MSB;
        answer_header[3] = LSB;
        // add key
        memcpy(&answer_header[6], key, key_length);

        send(sockfd, answer_header, key_length + 6, 0);
    } while ((ready = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == 0);

    unsigned char receive_header[1000];
    ready = recv(sockfd, &receive_header, 1000, 0);

    if (ready == -1)
        return -1; // error
    if (ready == 1)
        return 1; // acknowledged

    return 0;
}

int main(int argc, char *argv[])
{
    int error = 0;

    //check whether input is in good format
    if ((strcmp(argv[3], "GET") == 0 || strcmp(argv[3], "SET") == 0 || strcmp(argv[3], "DELETE") == 0) && (argc == 5 || argc == 6))
        error = 1;

    if (error != 1)
    {
        printf("Invalid arguments\n");
        exit(1);
    }

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
    if (getaddrinfo(ipdns, port, &client_info_config, &results) != 0)
    {
        errno = EINVAL;
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(sockfd, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(sockfd);

        errno = ECONNREFUSED;
        perror("connection error");
        exit(1);
    }
    if (strcmp(argv[3], "GET") == 0)
    {
        char *val = malloc(sizeof(char) * strlen(argv[4])); // TODO wrong size here or in function get?
        int valuelen = get(argv[4], strlen(argv[4]), &val);
        if (valuelen == -1)
        {
            errno = ENODATA;
            perror("GET Error \n");
            exit(1);
        }
        printf("%s\n", val); // TODO maybe wrong way to print
    }
    else if (strcmp(argv[3], "SET") == 0)
    {
        char *value = argv[5];
        if (set(argv[4], strlen(argv[4]), &value, strlen(value)) == -1)
        {
            errno = ENODATA;
            perror("SET Error\n");
            exit(1);
        }
    }
    else if (strcmp(argv[3], "DELETE") == 0)
    {
        if (del(argv[4], strlen(argv[4])) == -1)
        {
            errno = ENODATA;
            perror("DELETE Error\n");
            exit(1);
        }
    }

    close(sockfd);
    return 0;
}