#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "uthash.h"

#define HEAD 48

typedef struct database
{
    int key;
    char *value;
    UT_hash_handle hh;
} database;


//split header to different buffers
int main(int argc, char *argv[])
{

    //check for correct number of arguments
    if (argc != 1)
    {
        perror("Wrong input format");
        exit(1);
    }

    //port and filename
    char *port = argv[1];

    //declare needed structures
    database *db=NULL;
    struct addrinfo server_info_config;
    server_info_config = NULL;
    struct addrinfo *results;

    //initialize server_info_config
    memset(&server_info_config, 0, sizeof(server_info_config));
    server_info_config.ai_protocol = IPPROTO_TCP;
    server_info_config.ai_family = AF_INET;
    server_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo("", port, &server_info_config, &results) != 0)
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

    //start an infinite loop which monitors for new connections and handles it (the ~server~ program)
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

        //fork a new process to handle this client
        if (!fork())
        { //child process
            char receive_header[HEAD];
            char answer_header[HEAD];
            char transaction_id[8];
            char key_length_buf[16];
            char value_length_buf[16];
            int key_length;
            int value_length;
            char *ptr;

            //process the received header
            if (recv(client_socket, &receive_header, HEAD, 0) == -1) //receive header
            {
                perror("Error receiving ");
                exit(1);
            }

            memcpy(&transaction_id, receive_header[8], 8);    //copy transaction_id
            memcpy(&key_length_buf, receive_header[16], 16);   //copy key_length
            memcpy(&value_length_buf, receive_header[32], 16); //copy value_length

            //convert key_length and value_length
            key_length = atoi(key_length_buf);
            value_length = atoi(value_length_buf);

            char key[key_length];
            char value[value_length];

            //prepare the answer header
            memcpy(&answer_header, &receive_header, HEAD); //create template from received header
            memset(&answer_header, 0, 1);                  //clear up the first byte
            answer_header[4] = "1";
            
             if (recv(client_socket, &key, key_length, 0) == -1) //receive key
            {
                perror("Error receiving ");
                exit(1);
            }

             if(value_length>0){
                if (recv(client_socket, &value, value_length, 0) == -1) //receive value
            {
                perror("Error receiving ");
                exit(1);
            }
             }

            //break down header into different structures;
            if (receive_header[5] == 1)
            {   
                database *temp= malloc(sizeof(database));

                HASH_FIND_INT(db,key,temp);
                char *answer=temp->value;

            }

            else if (receive_header[6] == 1)
            {
                
            }

            else if (receive_header[7] == 1)
            {
                //delete
            }

            //prepare the answer header
            memcpy(&answer_header, &receive_header, HEAD); //create template from received header
            memset(&answer_header, 0, 4);                  //clear up the first 4 bytes 
            answer_header[4] = "1";

            //close the "listener" socket in child process - no need for child to wait monitor for new incomming connections
            close(server_socket);
            //after sending the quote, close the socket in child process
            close(client_socket);
            exit(0);
        }
        //this client has been already handled by the server, close this connection
        close(client_socket);
    }
    //clear the memory and close the file stream. this server doesn't support "safe quitting" though, needs to be killed by the OS
    freeaddrinfo(results);
}
