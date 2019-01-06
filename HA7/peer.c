#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "uthash.h"
#include <math.h>

#define HEAD 6

int yes = 1;
int fingertableflag = 1;
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
    int isBase;
};

struct finger_table_struct
{
    int index;
    struct id_add_port *node;
    UT_hash_handle hh;
};

struct transaction_hash_struct
{
    int transactionId;
    int hash;
    UT_hash_handle hh;
};

struct intern_hash_table_struct
{
    unsigned char *key;
    unsigned char *value;
    UT_hash_handle hh;
};

struct transaction_socket_struct
{
    int transactionId;
    int sockfd;
    UT_hash_handle hh;
};
struct finger_table_struct *fingertable = NULL;
struct intern_hash_table_struct *hashtable = NULL;
struct transaction_socket_struct *transock = NULL;
struct transaction_hash_struct *tranhash = NULL;
//------------------- Funktionen die Structs zurückgeben

struct intern_hash_table_struct *find_value(unsigned char key[])
{
    struct intern_hash_table_struct *s;
    HASH_FIND_STR(hashtable, key, s);
    return s;
}

struct transaction_socket_struct *find_socket(int transactionId)
{
    struct transaction_socket_struct *s;
    HASH_FIND_INT(transock, &transactionId, s);
    return s;
}

struct transaction_hash_struct *find_hash(int transactionId)
{
    struct transaction_hash_struct *s;
    HASH_FIND_INT(tranhash, &transactionId, s);
    return s;
}

struct finger_table_struct *find_finger(int index)
{
    struct finger_table_struct *s;
    HASH_FIND_INT(fingertable, &index, s);
    return s;
}

void add_finger(int index, struct id_add_port *node)
{
    printf("Node ID add: %u", node->id);
    struct finger_table_struct *s = NULL;
    s = (struct finger_table_struct *)malloc(sizeof *s);
    s->index = index;
    s->node = node;
    HASH_ADD_INT(fingertable, index, s); /* id: name of  transactionId field */
}

void add_hash(int transactionId, int hash)
{
    struct transaction_hash_struct *s = NULL;
    s = (struct transaction_hash_struct *)malloc(sizeof *s);
    s->transactionId = transactionId;
    s->hash = hash;
    HASH_ADD_INT(tranhash, transactionId, s); /* id: name of  transactionId field */
}

void delete_finger(struct finger_table_struct *s)
{
    HASH_DEL(fingertable, s);
    free(s);
}

void add_sockfd(int transactionId, int sockfd)
{
    if (transactionId != -1 && sockfd != -1)
    {
        struct transaction_socket_struct *s = NULL;
        s = (struct transaction_socket_struct *)malloc(sizeof *s);
        s->transactionId = transactionId;
        s->sockfd = sockfd;
        HASH_ADD_INT(transock, transactionId, s); /* id: name of  transactionId field */
    }
}

void delete_sockfd(struct transaction_socket_struct *s)
{
    HASH_DEL(transock, s);
    free(s);
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

void str_to_uint16(const unsigned char *str, uint16_t *res)
{
    char *end;
    long val = strtol(str, &end, 10);
    *res = (uint16_t)val;
}

int formula(uint16_t current_id, int i)
{
    return ((int)(current_id + pow(2.0, i)) % (int)(pow(2.0, 16.0)));
}

//------------------- Funktionen die void zurückgeben
void join_weiterleiten(unsigned char *receive_header, unsigned char *ip, unsigned char *port)
{
    unsigned char joinMessage[9];
    memcpy(joinMessage, receive_header, 9);

    //create a connection to next peer
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;
    //prepare peer_info_config
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;
    //get host information and load it into *results
    if (getaddrinfo(ip, port, &peer_info_config, &results) != 0)
    {
        errno = EINVAL;
        perror("getaddrinfo error join weiterleiten");
        exit(1);
    }
    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    setsockopt(nextPeersocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error join weiterleiten ");
        exit(1);
    }
    //send the message and close the socket
    send(nextPeersocket, &joinMessage, 9, 0);
    close(nextPeersocket);
}

void send_join(struct peer *toJoin)
{
    unsigned char joinMessage[9];
    joinMessage[0] = 0b11000000;
    //copy ID
    uint16_t ID = toJoin->current.id;
    uint16_t *ID_pointer = &ID;
    memcpy(joinMessage + 1, ID_pointer, 2);
    //copy IP
    struct in_addr in;
    in.s_addr = 0;
    char *ip = malloc(16);
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

    //create a connection to next peer
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //prepare peer_info_config
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(toJoin->nachfolger.add, toJoin->nachfolger.port, &peer_info_config, &results) != 0)
    {
        errno = EINVAL;
        perror("getaddrinfo error send join");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    setsockopt(nextPeersocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error send join");
        exit(1);
    }
    //send the message and close the socket
    send(nextPeersocket, &joinMessage, 9, 0);
    close(nextPeersocket);
}

void send_notify(unsigned char *ip_absender, unsigned char *port_absender, struct peer *current)
{
    if (current->vorganger.add != NULL)
    {
        unsigned char notifyMessage[11];
        notifyMessage[0] = 0b10100000;
        //copy ID
        uint16_t ID = current->vorganger.id;
        uint16_t *ID_pointer = &ID;
        memcpy(notifyMessage + 1, ID_pointer, 2);
        //copy IP
        struct in_addr in;
        in.s_addr = 0;
        char *ip = malloc(16);
        memcpy(ip, current->vorganger.add, strlen(current->vorganger.add));
        inet_aton(ip, &in);
        uint32_t IP = in.s_addr;
        uint32_t *IP_pointer = &IP;
        memcpy(notifyMessage + 3, IP_pointer, 4);
        //copy port
        uint16_t port;
        str_to_uint16(current->vorganger.port, &port);
        uint16_t *port_pointer = &port;
        memcpy(notifyMessage + 7, port_pointer, 2);

        //add ID of the notifier -> needed for |peers| = 2 to set nachfolger and vorganger appropriately
        uint16_t creator = current->current.id;
        uint16_t *creator_pointer = &creator;
        memcpy(notifyMessage + 9, creator_pointer, 2);

        //create a connection to next peer
        int nextPeersocket = 0;
        struct addrinfo peer_info_config;
        struct addrinfo *results;

        //prepare peer_info_config
        memset(&peer_info_config, 0, sizeof(peer_info_config));
        peer_info_config.ai_protocol = IPPROTO_TCP;
        peer_info_config.ai_family = AF_INET;
        peer_info_config.ai_socktype = SOCK_STREAM;

        //get host information and load it into *results
        if (getaddrinfo(ip_absender, port_absender, &peer_info_config, &results) != 0)
        {
            errno = EINVAL;
            perror("getaddrinfo error send notify");
            exit(1);
        }

        //create new socket using data obtained with getaddrinfo()
        nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
        setsockopt(nextPeersocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        //connect
        if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
        {
            close(nextPeersocket);
            errno = ECONNREFUSED;
            perror("connection error send notify");
            exit(1);
        }
        //send the message and close the socket
        send(nextPeersocket, &notifyMessage, 11, 0);
        close(nextPeersocket);
    }
}

void join(struct peer *current_peer, uint16_t id_absender, unsigned char *ip_absender, unsigned char *port_absender)
{
    //join a new node to the appropriate peer
    current_peer->vorganger.id = id_absender;
    current_peer->vorganger.add = calloc(strlen(ip_absender) + 1, 1);
    strcpy(current_peer->vorganger.add, ip_absender);
    current_peer->vorganger.port = calloc(strlen(port_absender) + 1, 1);
    strcpy(current_peer->vorganger.port, port_absender);
}

void send_stabilize(struct peer *current)
{
    if (current->nachfolger.add != NULL)
    {
        unsigned char stabilizeMessage[9];
        stabilizeMessage[0] = 0b10010000;
        //copy ID
        uint16_t ID = current->current.id;
        uint16_t *ID_pointer = &ID;
        memcpy(stabilizeMessage + 1, ID_pointer, 2);
        //copy IP
        struct in_addr in;
        in.s_addr = 0;
        char *ip = malloc(16);
        memcpy(ip, current->current.add, strlen(current->current.add));
        inet_aton(ip, &in);
        uint32_t IP = in.s_addr;
        uint32_t *IP_pointer = &IP;
        memcpy(stabilizeMessage + 3, IP_pointer, 4);
        //copy port
        uint16_t port;
        str_to_uint16(current->current.port, &port);
        uint16_t *port_pointer = &port;
        memcpy(stabilizeMessage + 7, port_pointer, 2);

        //create a connection to next peer
        int nextPeersocket = 0;
        struct addrinfo peer_info_config;
        struct addrinfo *results;

        //prepare peer_info_config
        memset(&peer_info_config, 0, sizeof(peer_info_config));
        peer_info_config.ai_protocol = IPPROTO_TCP;
        peer_info_config.ai_family = AF_INET;
        peer_info_config.ai_socktype = SOCK_STREAM;

        //get host information and load it into *results
        if (getaddrinfo(current->nachfolger.add, current->nachfolger.port, &peer_info_config, &results) != 0)
        {
            errno = EINVAL;
            perror("getaddrinfo error send stabilize");
            //exit(1);
        }

        //create new socket using data obtained with getaddrinfo()
        nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
        setsockopt(nextPeersocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        //connect
        if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
        {
            close(nextPeersocket);
            //errno = ECONNREFUSED;
            perror("connection error send stabilize");
            //exit(1);
        }
        //send the message and close the socket
        send(nextPeersocket, &stabilizeMessage, 9, 0);
        close(nextPeersocket);
    }
}

void *periodic_stabilize(void *arg)
{
    //send stabilize every 1 second
    while (1)
    {
        send_stabilize(arg);
        sleep(1);
    }
}

void *output_structure(void *arg)
{
    struct peer *current_peer = arg;
    time_t newtime;
    //print the peer configuration every 10 seconds
    while (1)
    {
        time(&newtime);
        if (newtime % 5 == 0)
        {
            printf("Vorgaenger:\n");
            printf("ID: %d\n", current_peer->vorganger.id);
            printf("IP: %s\n", current_peer->vorganger.add);
            printf("Port: %s\n", current_peer->vorganger.port);
            printf("Current:\n");
            printf("ID: %d\n", current_peer->current.id);
            printf("IP: %s\n", current_peer->current.add);
            printf("Port: %s\n", current_peer->current.port);
            printf("Nachfolger:\n");
            printf("ID: %d\n", current_peer->nachfolger.id);
            printf("IP: %s\n", current_peer->nachfolger.add);
            printf("port: %s\n\n", current_peer->nachfolger.port);
            sleep(3);
        }
    }
}

void send_to_client(unsigned char *receive_header, int client_socket, unsigned char *key, unsigned int key_length,
                    unsigned char *value, unsigned int value_length, int transactionId)
{

    unsigned char *answer = (unsigned char *)malloc(1000);
    memcpy(answer, receive_header, HEAD);
    memcpy(answer + 6 + key_length, value, value_length);
    memcpy(answer + 6, key, key_length);
    send(client_socket, answer, HEAD + key_length + value_length, 0);
    close(client_socket);
    struct transaction_socket_struct *found = find_socket(transactionId);
    if (found != NULL)
        delete_sockfd(found);
    free(answer);
}

void nachricht_ausgeben(struct peer *current_peer, int internal, int art, uint16_t id_absender,
                        unsigned char *ip_absender,
                        unsigned char *port_absender)
{
    printf("Internal : %i\n", internal);
    printf("Art der Nachricht : %d\n", art);
    printf("GET=1, SET=2, DELETE=3\n");
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

//TODO: NICHT nextPeerIp,port sondern peer in fingertable gucken
void nachricht_weiterleiten(unsigned char *nextPeerIP, unsigned char *nextPeerPort, unsigned char *receive_header,
                            unsigned char *answer_header,
                            unsigned int key_length, const unsigned char *key, const unsigned char *value,
                            unsigned int value_length, uint16_t id_absender, unsigned char *ip_absender,
                            unsigned char *port_absender)
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
        perror("getaddrinfo error nachricht weiterleiten");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    nextPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    setsockopt(nextPeersocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    //connect
    if (connect(nextPeersocket, results->ai_addr, results->ai_addrlen) == -1)
    {
        close(nextPeersocket);
        errno = ECONNREFUSED;
        perror("connection error nachricht weiterleiten");
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

void nachricht_bearbeiten(int client_socket, unsigned char *key, unsigned int key_length, unsigned char *value,
                          unsigned int value_length,
                          int art, unsigned char answer_header[], unsigned char transactionId,
                          unsigned char *ip_absender, unsigned char *port_absender, uint16_t id_absender,
                          unsigned char *receive_header, unsigned char *current_ip, unsigned char *current_port,
                          uint16_t current_id)
{
    memset(answer_header, 0, 1000);
    if (art == 1)
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
                send(client_socket, answer_header, 1000, 0);
                close(client_socket);
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
                send(client_socket, answer_header, 1000, 0);
                close(client_socket);
            }
        }
    }
    if (art == 2)
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
            send(client_socket, answer_header, 1000, 0);
            close(client_socket);
        }
    }
    if (art == 3)
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
            send(client_socket, answer_header, 1000, 0);
            close(client_socket);
        }
    }

    if (isNthBitSet(receive_header[0], 0))
    {
        nachricht_weiterleiten(ip_absender, port_absender, answer_header, answer_header, key_length, key, value,
                               value_length, current_id, current_ip, current_port);
    }
}

void read_header_peer(unsigned char **key, unsigned char *transactionId, uint16_t *id_absender,
                      unsigned char **ip_absender, unsigned char **port_absender,
                      unsigned char **value,
                      unsigned char *header, unsigned int *key_length, unsigned int *value_length)
{

    *key_length = (header[2] << 8) + header[3];
    *value_length = (header[4] << 8) + header[5];
    *transactionId = header[1];
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

void read_header_client(struct peer *current_peer, unsigned char **key, unsigned char *transactionId,
                        uint16_t *id_absender, unsigned char **ip_absender, unsigned char **port_absender,
                        unsigned char **value,
                        unsigned char *header, unsigned int *key_length, unsigned int *value_length)
{

    *key_length = (header[2] << 8) + header[3];
    *value_length = (header[4] << 8) + header[5];
    *transactionId = header[1];
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

void updateFingertable(struct id_add_port *peer_n, int hash, uint16_t current_id)
{
    for (int i = 0; i < 16; i++)
    {
        int start = formula(current_id, i);
        if (start >= hash && start <= peer_n->id)
        {
            add_finger(start, peer_n);
        }
        else
        {
            struct finger_table_struct *tempStruct = find_finger(start);
            if (tempStruct != NULL)
            {

                int eintrag = tempStruct->node->id;
                if (start > peer_n->id && eintrag < peer_n->id)
                {
                    add_finger(start, peer_n);
                }
            }
        }
    }
}

struct id_add_port *get_best_finger(int hashK, int current_id)
{
    struct id_add_port *bestFinger = malloc(sizeof(struct id_add_port));

    for (int i = 0; i < 16; i++)
    {
        int start = formula(current_id, i);
        struct finger_table_struct *tmp = find_finger(start);
        if (start <= hashK)
        { // TODO start oder tmp->node->id??
            //            copy information into bestFinger
            bestFinger->id = tmp->node->id;
            bestFinger->port = malloc(strlen(tmp->node->port) + 1);
            bestFinger->add = malloc(strlen(tmp->node->add) + 1);
            strcpy(bestFinger->add, tmp->node->add);
            strcpy(bestFinger->port, tmp->node->port);
        }
    }
    return bestFinger;
}

//------------------- Main
int main(int argc, unsigned char *argv[])
{
    struct peer *current_peer = calloc(1, sizeof(struct peer));
    //create new ring
    if (argc == 3 || argc == 4)
    {
        printf("create new ring\n");
        //initialisiere structs
        current_peer->current = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[1]) + 1), .port = malloc(strlen(argv[2]) + 1)};
        current_peer->isBase = 1;
        str_to_uint16(argv[3], &current_peer->current.id);
        sprintf(current_peer->current.add, "%.9s", argv[1]);
        sprintf(current_peer->current.port, "%.5s", argv[2]);
        if (argc == 4)
            str_to_uint16(argv[3], &current_peer->current.id);
    }
    //join an existing ring
    else if (argc == 6 || argc == 5)
    {
        //initialisiere structs
        current_peer->current = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[1]) + 1), .port = malloc(strlen(argv[2]) + 1)};
        current_peer->nachfolger = (struct id_add_port){.id = 0, .add = malloc(strlen(argv[4]) + 1), .port = malloc(strlen(argv[5]) + 1)};
        current_peer->isBase = 0;
        //copy id, addresses and ports
        str_to_uint16(argv[3], &current_peer->current.id);
        sprintf(current_peer->current.add, "%.9s", argv[1]);
        sprintf(current_peer->current.port, "%.5s", argv[2]);

        sprintf(current_peer->nachfolger.add, "%.9s", argv[4]);
        sprintf(current_peer->nachfolger.port, "%.5s", argv[5]);
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
    printf("%s %s", current_peer->current.add, current_peer->current.port);
    if (getaddrinfo(current_peer->current.add, current_peer->current.port, &server_info_config, &results) != 0)
    {
        perror("Error in getAddinfo main");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    int server_socket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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

    pthread_t thread_id[1];
    //create thread for sending stabilizing
    pthread_create(&thread_id[0], NULL, &periodic_stabilize, current_peer);
    //send join to existing ring
    if (argc == 6 || argc == 5)
        send_join(current_peer);
    //create thread for printing the structure
    pthread_create(&thread_id[1], NULL, &output_structure, current_peer);
    uint16_t current_id = current_peer->current.id;
    char *current_ip = (unsigned char *)malloc(strlen(current_peer->current.add) + 1);
    char *current_port = (unsigned char *)malloc(strlen(current_peer->current.port) + 1);
    strcpy(current_port, current_peer->current.port);
    strcpy(current_ip, current_peer->current.add);

    //create "blank" finger tables

    if (current_peer->isBase == 0)
    {
        for (int i = 0; i < 16; i++)
        {
            add_finger(formula(current_id, i), &current_peer->nachfolger);
        }
        for (int i = 0; i < 16; i++)
        {
            struct finger_table_struct *tmp = find_finger(formula(current_id, i));
            printf("ID %u \n", tmp->node->id);
        }
    }

    while (1)
    {
        struct sockaddr_storage client_info;
        socklen_t client_info_length = sizeof(client_info);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_info, &client_info_length);
        setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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

        int internal = isNthBitSet(receive_header[0], 0);
        int isJoin = isNthBitSet(receive_header[0], 1);
        int isNotify = isNthBitSet(receive_header[0], 2);
        int isStabilize = isNthBitSet(receive_header[0], 3);
        int art = 0;
        int artIsNull = 0;
        int isGet = isNthBitSet(receive_header[0], 5);
        int isSet = isNthBitSet(receive_header[0], 6);
        int isDelete = isNthBitSet(receive_header[0], 7);

        uint16_t id_absender;
        unsigned char *port_absender;
        unsigned char *ip_absender;
        unsigned int key_length;
        unsigned int value_length;
        unsigned char *key;
        unsigned char transactionId;
        unsigned char *value;

        if (isGet)
        {
            artIsNull = 1;
            art = 1;
        }
        if (isSet)
        {
            if (fingertableflag == 1)

            {
                if (!current_peer->isBase)
                {
                    for (int i = 0; i < 16; i++)
                    {
                        add_finger(formula(current_id, i), &current_peer->nachfolger);
                    }
                }
                fingertableflag = 0;
            }
            pthread_cancel(thread_id[0]);
            artIsNull = 1;
            art = 2;
        }
        if (isDelete)
        {
            artIsNull = 1;
            art = 3;
        }
        if (isJoin)
        {
            //printf("Joining \n");
            //copy ID,IP, and port
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

            //if there is only one peer in the ring, connect it with the joining node "in both direction" - nachfolger=vorganger
            if (current_peer->isBase == 1)
            {
                join(current_peer, id_absender, ip_absender, port_absender);
                current_peer->nachfolger.id = id_absender;
                current_peer->nachfolger.add = calloc(strlen(ip_absender) + 1, 1);
                strcpy(current_peer->nachfolger.add, ip_absender);
                current_peer->nachfolger.port = calloc(strlen(port_absender) + 1, 1);
                strcpy(current_peer->nachfolger.port, port_absender);
                current_peer->isBase = 0;

                //update the finger table for first peer
                if (current_peer->isBase)
                {
                    for (int i = 0; i < 16; i++)
                    {
                        add_finger(formula(current_id, i), &current_peer->nachfolger);
                    }
                }
            }
            //logic for finding the correct spot in the ring
            else if (current_peer->current.id < current_peer->vorganger.id)
            {
                if (id_absender > current_peer->current.id && id_absender > current_peer->vorganger.id)
                {
                    join(current_peer, id_absender, ip_absender, port_absender);
                }
                else if (id_absender <= current_peer->current.id && id_absender < current_peer->vorganger.id)
                {
                    join(current_peer, id_absender, ip_absender, port_absender);
                }
                else
                {
                    printf("Forwarding join request from node nr %d to node nr %d \n", current_peer->current.id,
                           current_peer->nachfolger.id);
                    join_weiterleiten(receive_header, current_peer->nachfolger.add, current_peer->nachfolger.port);
                }
            }
            else
            {
                if (id_absender <= current_peer->current.id && id_absender > current_peer->vorganger.id)
                {
                    join(current_peer, id_absender, ip_absender, port_absender);
                }
                else
                {

                    printf("Forwarding join request from node nr %d to node nr %d \n", current_peer->current.id,
                           current_peer->nachfolger.id);
                    join_weiterleiten(receive_header, current_peer->nachfolger.add, current_peer->nachfolger.port);
                }
            }
            close(client_socket);
            continue;
        }

        if (isNotify)
        {
            //printf("Notifying\n");
            //copy ID
            memcpy(&id_absender, receive_header + 1, 2);
            //copy IP
            struct in_addr in;
            int32_t ip_intrep;
            memcpy(&ip_intrep, receive_header + 3, 4);
            in.s_addr = ip_intrep;
            ip_absender = inet_ntoa(in);
            //copy port
            uint16_t port_intrep;
            memcpy(&port_intrep, receive_header + 7, 2);
            //printf("%d \n", port_intrep);
            port_absender = (unsigned char *)malloc(5);
            sprintf(port_absender, "%d", port_intrep);
            //printf("%s \n", portaddress);

            //copy the ID of the notifier to nachfolger - needed for |peers|=2
            //else the peer who joins doesn't store the ID of nachfolger, since it's by default initialized with 0
            memcpy(&current_peer->nachfolger.id, receive_header + 9, 2);
            if (current_peer->current.id != id_absender)
            {
                //printf("\n current id: %d, id received from notify: %d", current_peer->current.id, id_absender);
                current_peer->nachfolger.id = id_absender;
                current_peer->nachfolger.add = calloc(strlen(ip_absender) + 1, 1);
                strcpy(current_peer->nachfolger.add, ip_absender);
                current_peer->nachfolger.port = calloc(strlen(port_absender) + 1, 1);
                strcpy(current_peer->nachfolger.port, port_absender);
                /*printf("Nachfolger:\n");
                printf("ID aktualisiert: %d\n", current_peer->nachfolger.id);
                printf("IP aktualisiert : %s\n", current_peer->nachfolger.add);
                printf("port aktualisiert: %s\n", current_peer->nachfolger.port);            }
                else
                {
                    printf("Successor not changed\n");
                }
                */
            }
            close(client_socket);
            continue;
        }

        if (isStabilize)
        {
            //printf("Stabilizing\n");
            //copy ID
            memcpy(&id_absender, receive_header + 1, 2);
            //copy IP
            struct in_addr in;
            int32_t ip_intrep;
            memcpy(&ip_intrep, receive_header + 3, 4);
            in.s_addr = ip_intrep;
            ip_absender = inet_ntoa(in);
            //copy port
            uint16_t port_intrep;
            memcpy(&port_intrep, receive_header + 7, 2);
            //printf("%d \n", port_intrep);
            port_absender = (unsigned char *)malloc(5);
            sprintf(port_absender, "%d", port_intrep);
            //printf("%s \n", portaddress);
            /*printf("Vorgaenger:\n");
            printf("ID: %d\n", id_absender);
            printf("IP: %s\n", ip_absender);
            printf("port: %s\n", port_absender);*/
            //copy the ID,IP and port of the peer who sent stabilize to the receiver of the message - needed for |peers|=2
            //else the peer who just joined the ring can't send notify to anyone, since he doesn't have a vorganger
            if (current_peer->vorganger.add == NULL)
            {
                current_peer->vorganger.id = id_absender;
                current_peer->vorganger.add = calloc(strlen(ip_absender) + 1, 1);
                strcpy(current_peer->vorganger.add, ip_absender);
                current_peer->vorganger.port = calloc(strlen(port_absender) + 1, 1);
                strcpy(current_peer->vorganger.port, port_absender);
            }
            //send notify to the peer who sent stabilize
            send_notify(ip_absender, port_absender, current_peer);
            close(client_socket);
            continue;
        }

        if (artIsNull)
        {
            if (internal)
            {
                read_header_peer(&key, &transactionId, &id_absender, &ip_absender, &port_absender, &value,
                                 receive_header, &key_length, &value_length);
            }
            else
            {
                read_header_client(current_peer, &key, &transactionId, &id_absender, &ip_absender, &port_absender,
                                   &value, receive_header, &key_length, &value_length);
                add_sockfd(transactionId, client_socket);
                printf("%d, %d \n", transactionId, client_socket);
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
            //look for the most suitable node in the finger_table

            char *next_finger_ip;
            char *next_finger_port;
            struct id_add_port *next_finger = get_best_finger(hash_value, current_id);
            if (next_finger->add != NULL)
            {
                next_finger_ip = malloc(strlen(next_finger->add) + 1);
                next_finger_port = malloc(strlen(next_finger->port) + 1);
                strcpy(next_finger_ip, next_finger->add);
                strcpy(next_finger_port, next_finger->port);
            }

            if (isNthBitSet(receive_header[0], 4))
            {
                struct transaction_socket_struct *found = find_socket(transactionId);
                int csocket = found->sockfd;
                // struct transaction_hash_struct *found_hash = find_hash(transactionId);
                // int new_hash = found_hash->hash;

                //TODO read header in header stehen ip port id von dem knoten der das bearbeitet hat.
                //Create new finger with oben info
                struct id_add_port *peer_n = malloc(sizeof(struct id_add_port));
                peer_n->id = current_id;
                peer_n->port = malloc(strlen(current_port) + 1);
                peer_n->add = malloc(strlen(current_ip) + 1);
                strcpy(peer_n->add, current_ip);
                strcpy(peer_n->port, current_port);
                updateFingertable(peer_n, hash_value, current_id);
                send_to_client(receive_header, csocket, key, key_length, value, value_length, transactionId);
            }
            else if (current_peer->current.id < current_peer->vorganger.id)
            {
                //hash > last
                if (hash_value > current_peer->current.id && hash_value > current_peer->vorganger.id)
                {
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header,
                                         transactionId, ip_absender, port_absender, id_absender, receive_header,
                                         current_ip, current_port, current_id);
                }
                // hash € {0, current_peer_id}
                else if (hash_value <= current_peer->current.id && hash_value < current_peer->vorganger.id)
                {
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header,
                                         transactionId, ip_absender, port_absender, id_absender, receive_header,
                                         current_ip, current_port, current_id);
                }
                else
                {
                    //Nicht zuständig, dann weiter leiten
                    nachricht_weiterleiten(next_finger_ip, next_finger_port, receive_header, answer_header, key_length, key,
                                           value, value_length, id_absender, ip_absender, port_absender);
                }
            }
            else
            {
                //Fall 2: also normalen Fall
                if (hash_value <= current_peer->current.id && hash_value > current_peer->vorganger.id)
                {
                    //Dann current ist dafür zuständig
                    nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header,
                                         transactionId, ip_absender, port_absender, id_absender, receive_header,
                                         current_ip, current_port, current_id);
                }
                else
                {
                    nachricht_weiterleiten(next_finger_ip, next_finger_port, receive_header, answer_header, key_length, key,
                                           value, value_length, id_absender, ip_absender, port_absender);
                }
            }
        }
        if (internal == 1)
            close(client_socket);
    }
    free(current_peer);
    close(server_socket);
    freeaddrinfo(results);
}
