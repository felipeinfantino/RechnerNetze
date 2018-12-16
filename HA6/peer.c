#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "uthash.h"
#include <errno.h>

#define HEAD 6

int csocket = 0;
//------------------- Structs
struct id_add_port
{
    uint16_t id;
    unsigned char *add;
    unsigned char *port;
};

struct peer
{
    struct id_add_port current;
    struct id_add_port vorganger;
    struct id_add_port nachfolger;
};

struct intern_hash_table_struct
{
    unsigned char *key;
    unsigned char *value;
    UT_hash_handle hh;
};

struct intern_hash_table_struct *hashtable = NULL;

//------------------- Funktionen die Structs zurückgeben

struct intern_hash_table_struct *find_value(unsigned char key[])
{
    struct intern_hash_table_struct *s;
    HASH_FIND_STR(hashtable, key, s);
    return s;
}

//------------------- Funktionen die int zurückgeben

int isNthBitSet(unsigned char c, int n)
{
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}

unsigned int hash(const unsigned char *str, unsigned int length)
{
    unsigned int hash = 5381;
    unsigned int i = 0;

    for (i = 0; i < length; ++str, ++i)
    {
        hash = ((hash << 5) + hash) + (*str);
    }

    return hash;
}

//------------------- Void Funktionen

// Format der ausgabe wenn ein Peer eine Nachricht bekommt
// Internal : 0 oder 1
// Art der Nachricht : GET, SET oder DELETE
// Absender :
//   ID :
//   IP :
//   Port :
//
// Vorgänger :
//   ID :
//   IP :
//   Port :
//
// Nachfolger :
//   ID :
//   IP :

//   Port :

void send_to_client(unsigned char *receive_header, int client_socket, unsigned char *key, unsigned int key_length, unsigned char *value, unsigned int value_length)
{
    unsigned char *answer = (unsigned char *)malloc(1000);
    memcpy(answer, receive_header, HEAD);
    memcpy(answer + 6 + key_length, value, value_length);
    memcpy(answer + 6, key, key_length);
    send(client_socket, answer, HEAD + key_length + value_length, 0);
    close(client_socket);
    free(answer);
}

void nachricht_ausgeben(struct peer *current_peer, int internal, unsigned char *art, uint16_t id_absender, unsigned char *ip_absender,
                        unsigned char *port_absender)
{
    printf("Internal : %i\n", internal);
    printf("Art der Nachricht : %s\n", art);
    printf("Absender : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", id_absender, ip_absender, port_absender);
    printf("Vorgänger : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", current_peer->vorganger.id,
           current_peer->vorganger.add, current_peer->vorganger.port);
    printf("Nachfolger : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", current_peer->nachfolger.id,
           current_peer->nachfolger.add, current_peer->nachfolger.port);
}

void add_value(unsigned char key[], unsigned char value[])
{
    if (key != NULL && value != NULL)
    {
        struct intern_hash_table_struct *s = NULL;
        s = (struct intern_hash_table_struct *)malloc(sizeof *s);
        s->key = (unsigned char *)malloc(strlen(key) + 1);
        s->value = (unsigned char *)malloc(strlen(value) + 1);
        strcpy(s->key, key);
        strcpy(s->value, value);
        HASH_ADD_STR(hashtable, key, s); /* id: name of key field */
    }
}

void delete_value(struct intern_hash_table_struct *s)
{
    HASH_DEL(hashtable, s);
    free(s);
}

void str_to_uint16(const unsigned char *str, uint16_t *res)
{
    char *end;
    long val = strtol(str, &end, 10);
    *res = (uint16_t)val;
}

void nachricht_weiterleiten(unsigned char *nextPeerIP, unsigned char *nextPeerPort, unsigned char *receive_header, unsigned char *answer_header,
                            unsigned int key_length, const unsigned char *key, const unsigned char *value, unsigned int value_length, uint16_t id_absender, unsigned char *ip_absender, unsigned char *port_absender)
{
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //Convert IPv4 address to binary form
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(nextPeerIP, nextPeerPort, &peer_info_config, &results) != 0)

    {
        errno = EINVAL;
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error");
        exit(1);
    }

    memcpy(answer_header, receive_header, 6);
    //Fall 1 - current peer acts as the gate:
    if (!isNthBitSet(receive_header[0], 0))
        answer_header[0] += 128;
    //copy client header:

    uint16_t *ID_pointer = &id_absender;
    struct in_addr in;
    in.s_addr = 0;
    inet_aton(ip_absender, &in);
    uint32_t IP = in.s_addr;
    uint32_t *IP_pointer = &IP;
    uint16_t port;
    if (port_absender != NULL)
        port = atoi(port_absender);
    uint16_t *port_pointer = &port;
    memcpy(answer_header + 6, ID_pointer, 2);
    memcpy(answer_header + 8, IP_pointer, 4);
    memcpy(answer_header + 12, port_pointer, 2);
    memcpy(answer_header + 14 + key_length, value, value_length);
    memcpy(answer_header + 14, key, key_length);
    send(nextPeersocket, answer_header, 1000, 0);
    close(nextPeersocket);
}

void nachricht_bearbeiten(int clientsocket, unsigned char *key, unsigned int key_length, unsigned char *value, unsigned int value_length,
                          unsigned char *art, unsigned char answer_header[], unsigned char transactionId, unsigned char *ip_absender, unsigned char *port_absender, uint16_t id_absender, unsigned char *receive_header)
{
    memset(answer_header, 0, 1000);
    if (strcmp(art, "GET") == 0)
    {
        answer_header[0] = 0b00001100;
        answer_header[1] = transactionId;
        int length = key_length;
        int LSBMAX = 255;
        int LSB = key_length & LSBMAX;
        int MSB = (length - LSB) >> 8;
        answer_header[2] = MSB;
        answer_header[3] = LSB;
        struct intern_hash_table_struct *found = find_value(key);
        if (found == NULL)
        {
            value = 0;
            value_length = 0;
            if (!isNthBitSet(receive_header[0], 0))
            {
                memcpy(&answer_header[6], key, key_length);
                send(clientsocket, answer_header, 1000, 0);
                return;
            }
        }
        else
        {
            length = strlen(found->value);
            LSBMAX = 255;
            LSB = strlen(found->value) & LSBMAX;
            MSB = (length - LSB) >> 8;
            answer_header[4] = MSB;
            answer_header[5] = LSB;
            value = found->value;
            value_length = strlen(value);
            if (!isNthBitSet(receive_header[0], 0))
            {

                memcpy(&answer_header[6 + key_length], value, value_length);
                memcpy(&answer_header[6], key, key_length);
                send(clientsocket, answer_header, 1000, 0);
            }
        }
    }
    if (strcmp(art, "SET") == 0)
    {
        answer_header[0] = 0b00001010;
        answer_header[1] = transactionId;
        answer_header[2] = 0;
        answer_header[3] = 0;
        answer_header[4] = 0;
        answer_header[5] = 0;
        if (key != NULL && value != NULL)
            add_value(key, value);
        if (!isNthBitSet(receive_header[0], 0))
        {
            send(clientsocket, answer_header, 1000, 0);
        }
    }
    if (strcmp(art, "DELETE") == 0)
    {
        answer_header[0] = 0b00001001;
        answer_header[1] = transactionId;
        answer_header[2] = 0;
        answer_header[3] = 0;
        answer_header[4] = 0;
        answer_header[5] = 0;
        struct intern_hash_table_struct *found = find_value(key);
        if (found != NULL)
            delete_value(found);
        if (!isNthBitSet(receive_header[0], 0))
        {
            send(clientsocket, answer_header, 1000, 0);
        }
    }

    if (isNthBitSet(receive_header[0], 0))
    {
        nachricht_weiterleiten(ip_absender, port_absender, answer_header, answer_header, key_length, key, value, value_length, id_absender, ip_absender, port_absender);
    }
}

void read_header_peer(unsigned char **key, unsigned char *transaktions_id, uint16_t *id_absender, unsigned char **ip_absender, unsigned char **port_absender,
                      unsigned char **value,
                      unsigned char *header, unsigned int *key_length, unsigned int *value_length)
{

    *key_length = (header[2] << 8) + header[3];
    *value_length = (header[4] << 8) + header[5];
    *transaktions_id = header[1];
    *key = (unsigned char *)calloc(*key_length + 1, 1);
    *value = (unsigned char *)calloc(*value_length + 1, 1);

    //copy id:

    //copy and convert ip:
    struct in_addr in;
    int32_t ip_intrep;

    memcpy(&ip_intrep, &header[8], 4);
    in.s_addr = ip_intrep;
    unsigned char *ipaddress = inet_ntoa(in);
    *ip_absender = (unsigned char *)calloc((strlen(ipaddress) + 1), 1);
    memcpy(*ip_absender, ipaddress, strlen(ipaddress));
    memcpy(id_absender, header + 6, 2);
    //copy and convert port:
    uint16_t port_intrep;
    memcpy(&port_intrep, &header[12], 2);
    //printf("%d \n", port_intrep);
    unsigned char *portaddress = (unsigned char *)malloc(5);
    sprintf(portaddress, "%d", port_intrep);
    *port_absender = (unsigned char *)calloc(5, 5);
    //printf("%s \n", portaddress);
    memcpy(*port_absender, portaddress, strlen(portaddress));
    memcpy(*key, &header[14], *key_length);
    memcpy(*value, &header[14 + *key_length], *value_length);

    /*
    printf("Id absender : %u\n", *id_absender);
    printf("Port absender %s\n", *port_absender);
    printf("Ip absender : %s\n", *ip_absender);
    printf("keylength: %u", *key_length);
    printf("valuelength: %u", *value_length);
    printf("key: %s\n", *key);
    printf("value: %s\n", *value);

    //    key[*key_length] = '\0';
    //    value[*value_length] = '\0';
    */
}

void read_header_client(struct peer *current_peer, unsigned char **key, unsigned char *transaktions_id, uint16_t *id_absender, unsigned char **ip_absender, unsigned char **port_absender,
                        unsigned char **value,
                        unsigned char *header, unsigned int *key_length, unsigned int *value_length)
{

    *key_length = (header[2] << 8) + header[3];
    *value_length = (header[4] << 8) + header[5];
    *transaktions_id = header[1];
    *key = (unsigned char *)calloc(*key_length + 1, 1);
    *value = (unsigned char *)calloc(*value_length + 1, 1);
    *ip_absender = (unsigned char *)malloc(strlen(current_peer->current.add) + 1);
    *port_absender = (unsigned char *)malloc(strlen(current_peer->current.port) + 1);

    //Copy value and key
    memcpy(*key, &header[6], *key_length);
    memcpy(*value, &header[6 + *key_length], *value_length);

    //    key[*key_length] = '\0';
    //    value[*value_length] = '\0';

    // //Copy values in poiners
    memcpy(id_absender, &current_peer->current.id, 2);
    strcpy(*port_absender, current_peer->current.port);
    strcpy(*ip_absender, current_peer->current.add);
    /*
    printf("Id absender : %u\n", *id_absender);
    printf("Port absender %s\n", *port_absender);
    printf("Ip absender : %s\n", *ip_absender);
    printf("keylength: %u", *key_length);
    printf("valuelength: %u", *value_length);
    printf("key: %s\n", *key);
    printf("value: %s\n", *value);
    */
}

void send_join(struct peer *toJoin)
{

    printf("Enter send join nachricht\n");
    unsigned char joinMessage[9];
    joinMessage[0] = 0b11000000;
    //copy ID
    uint16_t ID = toJoin->current.id;
    uint16_t *ID_pointer = &ID;
    memcpy(joinMessage + 1, ID_pointer, 2);
    //copy IP
    struct in_addr in;
    in.s_addr = 0;
    char *ip = malloc(20);
    memcpy(ip, toJoin->current.add, strlen(toJoin->current.add));
    inet_aton(ip, &in);
    uint32_t IP = in.s_addr;
    uint32_t *IP_pointer = &IP;
    memcpy(joinMessage + 3, IP_pointer, 4);
    //copy port
    uint16_t port;
    str_to_uint16(toJoin->current.port, &port);
    uint16_t *port_pointer = &port;
    memcpy(joinMessage + 7, port_pointer, 2);

    //Mach verbindung mit nachfolger und schicke nur diese joinMessage
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //Convert IPv4 address to binary form
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(toJoin->nachfolger.add, toJoin->nachfolger.port, &peer_info_config, &results) != 0)

    {
        errno = EINVAL;
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error");
        exit(1);
    }
    printf("Sending nachricht\n");
    printf("%lu\n", send(nextPeersocket, &joinMessage, 9, 0));
}

void send_notify(unsigned char *ip_absender, unsigned char *port_absender, struct peer *toJoin)
{
    printf("Enter send notify nachricht\n");
    unsigned char joinMessage[9];
    joinMessage[0] = 0b10100000;
    //copy ID
    uint16_t ID = toJoin->vorganger.id;
    uint16_t *ID_pointer = &ID;
    memcpy(joinMessage + 1, ID_pointer, 2);
    //copy IP
    struct in_addr in;
    in.s_addr = 0;
    char *ip = malloc(20);
    memcpy(ip, toJoin->vorganger.add, strlen(toJoin->vorganger.add));
    inet_aton(ip, &in);
    uint32_t IP = in.s_addr;
    uint32_t *IP_pointer = &IP;
    memcpy(joinMessage + 3, IP_pointer, 4);
    //copy port
    uint16_t port;
    str_to_uint16(toJoin->vorganger.port, &port);
    uint16_t *port_pointer = &port;
    memcpy(joinMessage + 7, port_pointer, 2);

    //Mach verbindung mit nachfolger und schicke nur diese joinMessage
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //Convert IPv4 address to binary form
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(ip_absender, port_absender, &peer_info_config, &results) != 0)

    {
        errno = EINVAL;
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error");
        exit(1);
    }
    printf("%lu\n", send(nextPeersocket, &joinMessage, 9, 0));
}

void send_stabilize(struct peer *toJoin)
{

    printf("Enter send stabilize nachricht\n");
    unsigned char joinMessage[9];
    joinMessage[0] = 0b10010000;
    //copy ID
    uint16_t ID = toJoin->current.id;
    uint16_t *ID_pointer = &ID;
    memcpy(joinMessage + 1, ID_pointer, 2);
    //copy IP
    struct in_addr in;
    in.s_addr = 0;
    char *ip = malloc(20);
    memcpy(ip, toJoin->current.add, strlen(toJoin->current.add));
    inet_aton(ip, &in);
    uint32_t IP = in.s_addr;
    uint32_t *IP_pointer = &IP;
    memcpy(joinMessage + 3, IP_pointer, 4);
    //copy port
    uint16_t port;
    str_to_uint16(toJoin->current.port, &port);
    uint16_t *port_pointer = &port;
    memcpy(joinMessage + 7, port_pointer, 2);

    //Mach verbindung mit nachfolger und schicke nur diese joinMessage
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //Convert IPv4 address to binary form
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(toJoin->nachfolger.add, toJoin->nachfolger.port, &peer_info_config, &results) != 0)

    {
        errno = EINVAL;
        perror("getaddrinfo error");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error");
        exit(1);
    }
    printf("%lu\n", send(nextPeersocket, &joinMessage, 9, 0));
}

//------------------- Main

int main(int argc, unsigned char *argv[])
{

    //JOIN Argumente
    if (argc == 6 || argc == 5)
    {
        //Nach notify müssen die vorgänger und nachfolger befüllt
        struct peer *current_peer = malloc(sizeof(struct peer));
        //initialisiere structs
        current_peer->current = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[2]) + 1), .port = malloc(strlen(argv[3]) + 1)};
        current_peer->nachfolger = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[4]) + 1), .port = malloc(strlen(argv[5]) + 1)};

        sprintf(current_peer->current.add, "%.9s", argv[1]);
        sprintf(current_peer->current.port, "%.5s", argv[2]);
        //TODO da ID optional ist, muss dann geguckt werden ob es übergeben wurde oder nicht

        if (argc == 5)
        {
            current_peer->current.id = 0;
        }
        else
        {
            str_to_uint16(argv[3], &current_peer->current.id);
        }

        //Nachfolger TODO was machen wir mit der NachfolgerID , wieso soll man das nicht übergeben??
        //str_to_uint16(argv[7], &current_peer->nachfolger.id);
        sprintf(current_peer->nachfolger.add, "%.9s", argv[4]);
        sprintf(current_peer->nachfolger.port, "%.5s", argv[5]);

        send_join(current_peer);
    }
    else
    {
        if (argc != 10)
        {
            perror("Wrong input format");
            exit(1);
        }
        struct peer *current_peer = calloc(1, sizeof(struct peer));

        //initialisiere structs
        current_peer->current = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[2]) + 1), .port = malloc(strlen(argv[3]) + 1)};
        current_peer->vorganger = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[5]) + 1), .port = malloc(strlen(argv[6]) + 1)};
        current_peer->nachfolger = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[8]) + 1), .port = malloc(strlen(argv[9]) + 1)};

        //TODO sprintf to copy full adress
        // //current
        str_to_uint16(argv[1], &current_peer->current.id);
        sprintf(current_peer->current.add, "%.9s", argv[2]);
        sprintf(current_peer->current.port, "%.5s", argv[3]);

        // //Vorganger
        str_to_uint16(argv[4], &current_peer->vorganger.id);
        sprintf(current_peer->vorganger.add, "%.9s", argv[5]);
        sprintf(current_peer->vorganger.port, "%.5s", argv[6]);

        // //Nachfolger
        str_to_uint16(argv[7], &current_peer->nachfolger.id);
        sprintf(current_peer->nachfolger.add, "%.9s", argv[8]);
        sprintf(current_peer->nachfolger.port, "%.5s", argv[9]);

        if (current_peer->current.id == 0 || current_peer->vorganger.id == 0 || current_peer->nachfolger.id == 0)
        {
            perror("Id from one of the peers is 0, that is wrong");
            exit(1);
        }

        //declare needed structures
        struct addrinfo server_info_config;
        struct addrinfo *results;

        //initialize server_info_config
        memset(&server_info_config, 0, sizeof(server_info_config));
        server_info_config.ai_protocol = IPPROTO_TCP;
        server_info_config.ai_family = AF_INET;
        server_info_config.ai_socktype = SOCK_STREAM;

        //get host information and load it into *results
        if (getaddrinfo(current_peer->current.add, current_peer->current.port, &server_info_config, &results) != 0)
        {
            perror("Error in getAddinfo");
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

        while (1)
        {
            struct sockaddr_storage client_info;
            socklen_t client_info_length = sizeof(client_info);
            int client_socket = accept(server_socket, (struct sockaddr *)&client_info, &client_info_length);
            if (client_socket == -1)
            {
                perror("Error while accepting the connection");
                exit(1);
            }

            unsigned char receive_header[1000];
            unsigned char answer_header[1000];

            memset(receive_header, 0, 1000);
            memset(answer_header, 0, 1000);

            if (recv(client_socket, &receive_header, 1000, 0) == -1)
            {
                perror("Error receiving");
                exit(1);
            }

            printf("%d \n", receive_header[2]);

            int internal = isNthBitSet(receive_header[0], 0);
            int isJoin = isNthBitSet(receive_header[0], 1);
            int isNotify = isNthBitSet(receive_header[0], 2);
            int isStabilize = isNthBitSet(receive_header[0], 3);

            unsigned char *art = calloc(3, 3);
            int isGet = isNthBitSet(receive_header[0], 5);
            int isSet = isNthBitSet(receive_header[0], 6);
            int isDelete = isNthBitSet(receive_header[0], 7);

            if (isGet)
                strcpy(art, "GET");
            if (isSet)
                strcpy(art, "SET");
            if (isDelete)
                strcpy(art, "DELETE");

            if (art == NULL)
            {
                perror("Wrong usage, only set or get or delete.");
                exit(1);
            }

            unsigned int key_length;
            unsigned int value_length;
            unsigned char *key;
            unsigned char transaktions_id;
            uint16_t id_absender;
            unsigned char *port_absender;
            unsigned char *ip_absender;
            unsigned char *value;

            if (isJoin)
            {
                //Do the join stuff
                //copy ID
                memcpy(&id_absender, receive_header + 1, 2);

                struct in_addr in;
                int32_t ip_intrep;
                memcpy(&ip_intrep, receive_header + 3, 4);
                in.s_addr = ip_intrep;
                ip_absender = inet_ntoa(in);
                uint16_t port_intrep;
                memcpy(&port_intrep, receive_header + 7, 2);
                //printf("%d \n", port_intrep);
                port_absender = (unsigned char *)malloc(5);
                sprintf(port_absender, "%d", port_intrep);
                //printf("%s \n", portaddress);

                printf("ID: %d\n", id_absender);
                printf("IP: %s\n", ip_absender);
                printf("port: %s\n", port_absender);

                printf("Joining\n");
                current_peer->vorganger.id = id_absender;
                current_peer->vorganger.add = calloc(strlen(ip_absender) + 1, 1);
                strcpy(current_peer->vorganger.add, ip_absender);
                current_peer->vorganger.port = calloc(strlen(port_absender) + 1, 1);
                strcpy(current_peer->vorganger.port, port_absender);

                printf("ID aktualisiert: %d\n", current_peer->vorganger.id);
                printf("IP aktualisiert : %s\n", current_peer->vorganger.add);
                printf("port aktualisiert: %s\n", current_peer->vorganger.port);

                continue; //continue weil wir wollen keine header lesen und auch keine Nachricht weiterleiten bzw bearbeiten
            }

            if (isNotify)
            {
                printf("Notifying\n");
                //Do the notify stuff
                //copy ID
                memcpy(&id_absender, receive_header + 1, 2);

                struct in_addr in;
                int32_t ip_intrep;
                memcpy(&ip_intrep, receive_header + 3, 4);
                in.s_addr = ip_intrep;
                ip_absender = inet_ntoa(in);
                uint16_t port_intrep;
                memcpy(&port_intrep, receive_header + 7, 2);
                //printf("%d \n", port_intrep);
                port_absender = (unsigned char *)malloc(5);
                sprintf(port_absender, "%d", port_intrep);
                //printf("%s \n", portaddress);

                printf("ID: %d\n", id_absender);
                printf("IP: %s\n", ip_absender);
                printf("port: %s\n", port_absender);
                if(current_peer->nachfolger.id != id_absender){
                    current_peer->nachfolger.id = id_absender;
                    current_peer->nachfolger.add = calloc(strlen(ip_absender) + 1, 1);
                    strcpy(current_peer->nachfolger.add, ip_absender);
                    current_peer->nachfolger.port = calloc(strlen(port_absender) + 1, 1);
                    strcpy(current_peer->nachfolger.port, port_absender);

                    printf("ID aktualisiert: %d\n", current_peer->nachfolger.id);
                    printf("IP aktualisiert : %s\n", current_peer->nachfolger.add);
                    printf("port aktualisiert: %s\n", current_peer->nachfolger.port);
                }else {
                    printf("still my successor\n");
                }
                continue; //continue weil wir wollen keine header lesen und auch keine Nachricht weiterleiten bzw bearbeiten
            }

            if (isStabilize)
            {
                printf("Stabilizing\n");
                //Do the stabilize stuff
                //copy ID
                memcpy(&id_absender, receive_header + 1, 2);

                struct in_addr in;
                int32_t ip_intrep;
                memcpy(&ip_intrep, receive_header + 3, 4);
                in.s_addr = ip_intrep;
                ip_absender = inet_ntoa(in);
                uint16_t port_intrep;
                memcpy(&port_intrep, receive_header + 7, 2);
                //printf("%d \n", port_intrep);
                port_absender = (unsigned char *)malloc(5);
                sprintf(port_absender, "%d", port_intrep);
                //printf("%s \n", portaddress);

                printf("ID: %d\n", id_absender);
                printf("IP: %s\n", ip_absender);
                printf("port: %s\n", port_absender);

                send_notify(ip_absender, port_absender, current_peer);
                continue; //continue weil wir wollen keine header lesen und auch keine Nachricht weiterleiten bzw bearbeiten
            }

            if (internal)
            {
                read_header_peer(&key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value, receive_header, &key_length, &value_length);
            }
            else
            {
                csocket = client_socket;
                read_header_client(current_peer, &key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value, receive_header, &key_length, &value_length);
            }
            nachricht_ausgeben(current_peer, internal, art, id_absender, ip_absender, port_absender);

            // //Hash der Key und gucke ob das zu dem aktuellen peer gehört
            // mod % 2^16, da es der wertebereich ist

            unsigned char *nextPeerIP = current_peer->nachfolger.add;
            unsigned char *nextPeerPort = current_peer->nachfolger.port;
            unsigned int hash_value = (hash(key, (key_length))) % 65536;

            //Prüfen ob den aktuellen peer zuständig für die anfrage ist
            //Wenn ja, bearbeite die anfrage (also führe set, get, delete in interne hastable aus)
            //Wenn nicht setzte das intern bit zu 1 und leite dass zu nachfolger weiter

            // 2 Fälle
            // Fall 1 : hash liegt zwischen den letzen und dem ersten Knoten
            // Bsp letzte knoten 220 und ersten 10
            // Fall 2 : hash liegt zwischen zwei normalen knoten.

            //Fall 1 (ersten Knoten): wir gucken ob der current peer ein kleineres ID hat als vorgänger, wenn ja es ist Fall 1 ansonsten Fall 2

            if (isNthBitSet(receive_header[0], 4))
            {
                send_to_client(receive_header, csocket, key, key_length, value, value_length);
            }
            else if (current_peer->current.id < current_peer->vorganger.id)
            {
                //hash > last
                if (hash_value > current_peer->current.id && hash_value > current_peer->vorganger.id)
                {
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header, transaktions_id, ip_absender, port_absender, id_absender, receive_header);
                }
                // hash € {0, current_peer_id}
                else if (hash_value <= current_peer->current.id && hash_value < current_peer->vorganger.id)
                {
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header, transaktions_id, ip_absender, port_absender, id_absender, receive_header);
                }
                else
                {
                    //Nicht zuständig, dann weiter leiten
                    nachricht_weiterleiten(nextPeerIP, nextPeerPort, receive_header, answer_header, key_length, key, value, value_length, id_absender, ip_absender, port_absender);
                }
            }
            else
            {
                //Fall 2: also normalen Fall
                if (hash_value <= current_peer->current.id && hash_value > current_peer->vorganger.id)
                {
                    //Dann current ist dafür zuständig
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header, transaktions_id, ip_absender, port_absender, id_absender, receive_header);
                }
                else
                {
                    nachricht_weiterleiten(nextPeerIP, nextPeerPort, receive_header, answer_header, key_length, key, value, value_length, id_absender, ip_absender, port_absender);
                }
            }
            if (internal == 1)
                close(client_socket);
            free(key);
            free(value);
        }
        free(current_peer);
        close(server_socket);
        freeaddrinfo(results);
    }
}