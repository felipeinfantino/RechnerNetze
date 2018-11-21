#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/time.h>

int set(char *key, int key_length, char *value, int value_length)
{
}

int get(char *key, int key_length, char **value)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockdfd);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ready;
    do
    {
        send_get((key, keylen));
    } while ((ready = select(sockdfd + 1, &rfds, NULL, NULL, &tv)) == 0);

    return -1;
}

int delete (char *key, int key_length)
{
}

int main(int argc, char *argv[])
{

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
        perror("");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    int sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(sockfd, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("connection error");
    }

    unsigned char receive_header[1000];
    unsigned char answer_header[1000];

    if (argv[4] == "GET")
    {
        char *val;
        int valuelen = get(key, keylen, &val);
        if (valuelen == -1)
        {
            perror("GET Error \n");
            exit(1);
        }
    }

    else if (argv[4] == "SET")
    {
        if (set(key, keylen, val, valuelen) == -1)
        {
            perror("SET Error\n");
            exit(1);
        }
    }
    else if (argv[4] == "DELETE")
    {
        if (delete (key, keylen, val, valuelen) == -1)
        {
            perror("DELETE Error\n");
            exit(1);
        }
    }

    close(sockfd);
    return 0;
}

int key_length = (receive_header[2] << 8) + receive_header[3];   //process the key length
int value_length = (receive_header[4] << 8) + receive_header[5]; //process the value length
char key[key_length + 1];
char value[value_length + 1];

memcpy(&key[0], &receive_header[6], key_length);
memcpy(&value[0], &receive_header[6 + key_length], value_length);
memcpy(answer_header, receive_header, 1000); //create template from received header

key[key_length] = '\0';
value[value_length] = '\0';
//exceute the correct function
if (receive_header[0] == 4) //GET
{
    struct my_struct *found = find_value(key);
    if (found == NULL)
    {
        printf("VALUE: %s not found\n", value);
        char delAnswer[1000];
        memset(delAnswer, 0, 1000);
        delAnswer[0] = 0b00001100;
        delAnswer[1] = receive_header[1];
        send(client_socket, delAnswer, 1000, 0);
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
        send(client_socket, answer_header, 1000, 0);
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
    char delAnswer[1000];
    memset(delAnswer, 0, 1000);
    delAnswer[0] = 0b00001010;
    delAnswer[1] = receive_header[1];
    send(client_socket, delAnswer, 1000, 0);
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

        char delAnswer[1000];
        memset(delAnswer, 0, 1000);
        delAnswer[0] = 0b00001001;
        delAnswer[1] = receive_header[1];
        send(client_socket, delAnswer, 1000, 0);
    }
    else
    {
        char delAnswer[1000];
        memset(delAnswer, 0, 1000);
        delAnswer[0] = 0b00001001;
        delAnswer[1] = receive_header[1];
        send(client_socket, delAnswer, 1000, 0);
    }
}
close(client_socket);
}
close(sockfd);
freeaddrinfo(results);
}
