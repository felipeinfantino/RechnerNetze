#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "uthash.h"

#define HEAD 6

/* structure for storing <key,value>
typedef struct database
{
    char key[30];
    char value[30];
    UT_hash_handle hh;
} database;

*/

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

        unsigned char receive_header[HEAD];
        unsigned char answer_header[HEAD];

        if (recv(client_socket, &receive_header, HEAD, 0) == -1) //receive header
        {
            perror("Error receiving ");
            exit(1);
        }

        int key_length = (receive_header[2] << 8) + receive_header[3];   //process the key length
        int value_length = (receive_header[4] << 8) + receive_header[5]; //process the value length
        char key[key_length];   
        char value[value_length];

        memcpy(answer_header, receive_header, HEAD); //create template from received header

        if (recv(client_socket, &key, key_length, 0) == -1) //receive key
        {
            perror("Error receiving ");
            exit(1);
        }

        if (value_length > 0)
        {
            if (recv(client_socket, &value, value_length, 0) == -1) //receive value
            {
                perror("Error receiving ");
                exit(1);
            }
        }

        //exceute the correct function
        if (receive_header[0] == 4) //GET
        {

            answer_header[0] = 0b00001100;
            send(client_socket, answer_header, HEAD, 0);
            send(client_socket, key, sizeof(key), 0);
            send(client_socket, value, sizeof(value), 0);
        }
        else if (receive_header[0] == 2) //SET
        {
            /*
            database *s = malloc(sizeof(database));
            strcpy(s->key, key);
            strcpy(s->value, value);
            HASH_ADD_STR(db, key, s);
            */
            answer_header[0] = 0b00001010;
            send(client_socket, answer_header, HEAD, 0);
        }
        else if (receive_header[0] == 1) //DELETE
        {

            answer_header[0] = 0b00001001;
            send(client_socket, answer_header, HEAD, 0);
        }
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}
