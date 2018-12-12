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

void send_to_client(unsigned char *receive_header, unsigned char *answer_header, int client_socket)
{
    memset(answer_header, 0, 1000);
    memcpy(answer_header, receive_header, HEAD);
    send(client_socket, receive_header, HEAD, 0);
    close(client_socket);
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
    struct intern_hash_table_struct *s = NULL;
    s = (struct intern_hash_table_struct *)malloc(sizeof *s);
    s->key = (unsigned char *)malloc(strlen(key) + 1);
    s->value = (unsigned char *)malloc(strlen(value) + 1);
    strcpy(s->key, key);
    strcpy(s->value, value);
    HASH_ADD_STR(hashtable, key, s); /* id: name of key field */
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

    //Fall 1 - current peer acts as the gate:
    if (!isNthBitSet(receive_header[0], 0) && !isNthBitSet(receive_header[0], 4))
    {
        printf("Fall 1\n");
        //copy client header:
        memcpy(answer_header, receive_header, 6);
        answer_header[0] += 128;
        uint16_t *ID_pointer = &id_absender;

        struct in_addr in;
        inet_aton(ip_absender, &in);
        uint32_t IP = in.s_addr;
        void *IP_pointer = &IP;
        uint16_t port = atoi(port_absender);
        void *port_pointer = &port;
        memcpy(answer_header + 6, ID_pointer, 2);
        memcpy(answer_header + 8, IP_pointer, 4);
        memcpy(answer_header + 12, port_pointer, 2);
    }
    else
        //copy whole header
        memcpy(answer_header, receive_header, 14);
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
            if (!isNthBitSet(receive_header[0], 0))
            {
                memcpy(&answer_header[6], key, key_length);
                send(clientsocket, answer_header, 1000, 0);
                return;
            }
        }
        // add value_length to answer
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
    if (strcmp(art, "SET") == 0)
    {
        answer_header[0] = 0b00001010;
        answer_header[1] = transactionId;
        answer_header[2] = 0;
        answer_header[3] = 0;
        answer_header[4] = 0;
        answer_header[5] = 0;

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
        answer_header[0] += 128;
        nachricht_weiterleiten(ip_absender, port_absender, receive_header, answer_header, key_length, key, value, value_length, id_absender, ip_absender, port_absender);
    }
}

void read_header_peer(unsigned char **key, unsigned char *transaktions_id, uint16_t *id_absender, unsigned char **ip_absender, unsigned char **port_absender,
                      unsigned char **value,
                      unsigned char *header, unsigned int *key_length, unsigned int *value_length)
{

    *key_length = (header[2] << 8) + header[3];
    *value_length = (header[4] << 8) + header[5];
    *transaktions_id = header[1];
    *key = malloc(*key_length + 1);
    *value = malloc(*value_length + 1);

    //copy id:

    //copy and convert ip:
    struct in_addr in;
    int32_t ip_intrep;

    memcpy(&ip_intrep, &header[8], 4);
    in.s_addr = ip_intrep;
    char *ipaddress = inet_ntoa(in);
    *ip_absender = malloc(strlen(ipaddress));
    memcpy(*ip_absender, ipaddress, strlen(ipaddress));
    memcpy(id_absender, header + 6, 2);
    //copy and convert port:
    uint16_t port_intrep;
    memcpy(&port_intrep, &header[12], 2);
    printf("%d \n", port_intrep);
    //char *portaddress;
    ///sprintf(portaddress, "%d", port_intrep);
    //memcpy(*port_absender, portaddress, strlen(portaddress));

    memcpy(*key, &header[14], *key_length);
    memcpy(*value, &header[14 + *key_length], *value_length);

    /*
    //inet_ntop(AF_INET, ip_absender, ipaddr, INET_ADDRSTRLEN);
    printf("Id absender : %u\n", *id_absender);
    //printf("Port absender %s\n", *port_absender);
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
    *key = malloc(*key_length + 1);
    *value = malloc(*value_length + 1);
    *ip_absender = malloc(strlen(current_peer->current.add) + 1);
    *port_absender = malloc(strlen(current_peer->current.port) + 1);

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

//------------------- Main

int main(int argc, unsigned char *argv[])
{
    if (argc != 10)
    {
        perror("Wrong input format");
        exit(1);
    }
    struct peer *current_peer = malloc(sizeof(struct peer));

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

        if (recv(client_socket, &receive_header, 1000, 0) == -1)
        {
            perror("Error receiving");
            exit(1);
        }

        int internal = isNthBitSet(receive_header[0], 0);

        unsigned char *art;
        int isGet = isNthBitSet(receive_header[0], 5);
        int isSet = isNthBitSet(receive_header[0], 6);
        int isDelete = isNthBitSet(receive_header[0], 7);

        if (isGet)
        {
            art = calloc(3, 3);
            strcpy(art, "GET");
        }
        if (isSet)
        {
            art = calloc(3, 3);
            strcpy(art, "SET");
        }
        if (isDelete)
        {
            art = calloc(6, 6);
            strcpy(art, "DELETE");
        }

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

        printf("CURRENT ID ABSENDER: %d", id_absender);
        //Wenn es intern ist, dann lesen wir read_header_peer, andersfalls lesen wir read_header_client
        if (internal == 0 && isNthBitSet(receive_header[0], 4))
        {
            printf("(internal == 0 && isNthBitSet(receive_header[0], 4) \n");
            send_to_client(receive_header, answer_header, csocket);
        }
        else if (internal)
        {
            printf("internal \n");
            read_header_peer(&key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value, receive_header, (&key_length), (&value_length));
        }
        else
        {
            printf("else \n");
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

        if (current_peer->current.id < current_peer->vorganger.id)
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
                //Nicht zuständig, dann weiter leiten
                nachricht_weiterleiten(nextPeerIP, nextPeerPort, receive_header, answer_header, key_length, key, value, value_length, id_absender, ip_absender, port_absender);
            }
        }
        //if (internal == 1)
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}