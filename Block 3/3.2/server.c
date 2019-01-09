#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "uthash.h"

#define HEAD 6

struct my_struct
{
    char key[100000];
    char value[100000];
    UT_hash_handle hh;
};

struct my_struct *hashtable = NULL;

void add_value(char key[], char value[])
{
    struct my_struct *s = NULL;
    s = (struct my_struct *)malloc(sizeof *s);
    strcpy(s->key, key);
    strcpy(s->value, value);
    HASH_ADD_STR(hashtable, key, s); /* id: name of key field */
}

struct my_struct *find_value(char key[])
{
    struct my_struct *s;
    HASH_FIND_STR(hashtable, key, s);
    return s;
}

void delete_value(struct my_struct *s)
{
    HASH_DEL(hashtable, s);
    free(s);
}

//split header to different buffers
int main(int argc, char *argv[])
{

    //check for correct number of arguments
    if (argc != 2)
    {
        perror("Wrong input format");
        exit(1);
    }

    //port and filename
    char *port = argv[1];

    //declare needed structures
    struct addrinfo server_info_config;
    struct addrinfo *results;

    //initialize server_info_config
    memset(&server_info_config, 0, sizeof(server_info_config));
    server_info_config.ai_protocol = IPPROTO_TCP;
    server_info_config.ai_family = AF_INET;
    server_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo("localhost", port, &server_info_config, &results) != 0)
    {
        perror("");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    int server_socket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //bind the new socket to a port
    if (bind(server_socket, results->ai_addr, results->ai_addrlen) == -1)
    {
        perror("Couldn't bind the socket to this port");
        exit(1);
    }

    //prepare the socket for incoming connections
    if (listen(server_socket, 10) == -1)
    {
        perror("Error while listening");
        exit(1);
    }

    //start the infinite loop
    while (1)
    {
        //construct sockaddr_storage for storing client information
        struct sockaddr_storage client_info;
        socklen_t client_info_length = sizeof(client_info);

        //aceept the newest connection and load it's data to the client_info struct
        int client_socket = accept(server_socket, (struct sockaddr *)&client_info, &client_info_length);
        if (client_socket == -1)
        {
            perror("Error while accepting the connection");
            exit(1);
        }

        unsigned char receive_header[200000];
        unsigned char answer_header[200000];

        if (recv(client_socket, &receive_header, 200000, 0) == -1) //receive header
        {
            perror("Error receiving ");
            exit(1);
        }

        int key_length = (receive_header[2] << 8) + receive_header[3];   //process the key length
        int value_length = (receive_header[4] << 8) + receive_header[5]; //process the value length
        char key[key_length + 1];
        char value[value_length + 1];

        memcpy(&key[0], &receive_header[6], key_length);
        memcpy(&value[0], &receive_header[6 + key_length], value_length);
        memcpy(answer_header, receive_header, 200000); //create template from received header

        key[key_length] = '\0';
        value[value_length] = '\0';
        //exceute the correct function
        if (receive_header[0] == 4) //GET
        {
            struct my_struct *found = find_value(key);
            if (found == NULL)
            {
                printf("VALUE: %s not found\n", value);
                char delAnswer[200000];
                memset(delAnswer, 0, 200000);
                delAnswer[0] = 0b00001100;
                delAnswer[1] = receive_header[1];
                send(client_socket, delAnswer, 200000, 0);
            }
            else
            {
                int length = strlen(found->value);
                int LSBMAX = 255;
                int LSB = strlen(found->value) & LSBMAX;
                int MSB = (length - LSB) >> 8;
                answer_header[4] = MSB;
                answer_header[5] = LSB;

                memcpy(&answer_header[6 + key_length], found->value, strlen(found->value));

                printf("Found KEY: %s with VALUE: %s\n", found->key, found->value);
                answer_header[0] = 0b00001100;
                answer_header[1] = receive_header[1];
                send(client_socket, answer_header, 200000, 0);
            }
        }
        else if (receive_header[0] == 2) //SET
        {

            add_value(key, value); //WITH UTHASH

            struct my_struct *found = find_value(key);
            if (found == NULL)
            {
                printf(" hiii");
            }
            else
            {
            }
            printf("SET KEY: %s with VALUE: %s\n", key, value);
            char delAnswer[200000];
            memset(delAnswer, 0, 200000);
            delAnswer[0] = 0b00001010;
            delAnswer[1] = receive_header[1];
            send(client_socket, delAnswer, 200000, 0);
        }
        else if (receive_header[0] == 1) //DELETE
        {
            struct my_struct *toDel = (struct my_struct *)malloc(sizeof *toDel);

            struct my_struct *found = find_value(key);
            if (found != NULL)
            {

                strcpy(toDel->key, key);
                strcpy(toDel->value, value);
                printf("DELETED VALUE: %s\n", key);
                delete_value(toDel);

                char delAnswer[200000];
                memset(delAnswer, 0, 200000);
                delAnswer[0] = 0b00001001;
                delAnswer[1] = receive_header[1];
                send(client_socket, delAnswer, 200000, 0);
            }
            else
            {
                char delAnswer[200000];
                memset(delAnswer, 0, 200000);
                delAnswer[0] = 0b00001001;
                delAnswer[1] = receive_header[1];
                send(client_socket, delAnswer, 200000, 0);
            }
        }
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}
