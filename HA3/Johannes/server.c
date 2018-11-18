#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "uthash.h"

#define HEAD 6


struct my_struct {
    char *key;
    char *value;
    UT_hash_handle hh;
};

struct my_struct *hashtable = NULL;

void add_value(char key[], char value[]) {
    struct my_struct *s = NULL;
    HASH_FIND_STR(hashtable, key, s);  /* id already in the hash? */
    if (s==NULL) {
        s = (struct my_struct *)malloc(sizeof *s);
        s->key = (char *) malloc(strlen(key) + 1);
        s->value = (char *) malloc(strlen(value) + 1);
        strncpy(s->key, key, strlen(key));
        strncpy(s->value, value, strlen(value));
        HASH_ADD_STR(hashtable, key, s );  /* id: name of key field */
    }
}

struct my_struct *find_value(char key[]) {
    struct my_struct *s;
    HASH_FIND_STR(hashtable, key, s );
    return s;
}

void delete_value(struct my_struct *s) {
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

        unsigned char receive_header[1000];
        unsigned char answer_header[1000];

        if (recv(client_socket, &receive_header, 1000, 0) == -1) //receive header
        {
            perror("Error receiving ");
            exit(1);
        }

        int key_length = (receive_header[2] << 8) + receive_header[3];   //process the key length
        int value_length = (receive_header[4] << 8) + receive_header[5]; //process the value length
        char key[key_length];
        char value[value_length];

        memcpy(&key[0],&receive_header[6], key_length);
        memcpy(&value[0],&receive_header[6+key_length], value_length);
        memcpy(answer_header, receive_header, 1000); //create template from received header

        //exceute the correct function
        if (receive_header[0] == 4) //GET
        {
            struct my_struct *found = find_value(key);
            if(found == NULL) {
                printf("VALUE: %s not found\n", value);
                char delAnswer[1000];
                memset(delAnswer, 0, 1000);
                delAnswer[0] = 0b00001100;
                delAnswer[1] = receive_header[1];
                send(client_socket, delAnswer, 1000, 0);
            } else {
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
                send(client_socket, answer_header, 1000, 0);
            }
        }
        else if (receive_header[0] == 2) //SET
        {
            add_value(key, value); //WITH UTHASH
            printf("SET KEY: %s with VALUE: %s\n", key, value);
            char delAnswer[1000];
            memset(delAnswer, 0, 1000);
            delAnswer[0] = 0b00001010;
            delAnswer[1] = receive_header[1];
            send(client_socket, delAnswer, 1000, 0);
        }
        else if (receive_header[0] == 1) //DELETE
        {
            struct my_struct *toDel = (struct my_struct *)malloc(sizeof *toDel);
            toDel->key = (char *) malloc(strlen(key) + 1);
            toDel->value = (char *) malloc(strlen(value) + 1);
            strncpy(toDel->key, key, strlen(key));
            strncpy(toDel->value, value, strlen(value));
            delete_value(toDel);
            printf("DELETED VALUE: %s\n", value);
            char delAnswer[1000];
            memset(delAnswer, 0, 1000);
            delAnswer[0] = 0b00001001;
            delAnswer[1] = receive_header[1];
            send(client_socket, delAnswer, 1000, 0);
        }
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}
