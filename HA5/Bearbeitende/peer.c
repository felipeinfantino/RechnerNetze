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

//------------------- Structs
struct id_add_port
{
    uint16_t id;
    char *add;
    char* port;
};

struct peer
{
    struct id_add_port current;
    struct id_add_port vorganger;
    struct id_add_port nachfolger;
};

struct intern_hash_table_struct
{
    char *key;
    char *value;
    UT_hash_handle hh;
};

struct intern_hash_table_struct *hashtable = NULL;

//------------------- Funktionen die Structs zurückgeben

struct intern_hash_table_struct *find_value(char key[])
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

unsigned int hash(const char *str, unsigned int length)
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
void nachricht_ausgeben(struct peer *current_peer, int internal, char *art, uint16_t id_absender, char *ip_absender,
                        char* port_absender)
{
    printf("Internal : %i\n", internal);
    printf("Art der Nachricht : %s\n", art);
    printf("Absender : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", id_absender, ip_absender, port_absender);
    printf("Vorgänger : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", current_peer->vorganger.id,
           current_peer->vorganger.add, current_peer->vorganger.port);
    printf("Nachfolger : \n\t ID : %u\n\t IP : %s\n\t PORT : %s\n\n", current_peer->nachfolger.id,
           current_peer->nachfolger.add, current_peer->nachfolger.port);
}

void add_value(char key[], char value[])
{
        struct intern_hash_table_struct *s = NULL;
        s = (struct intern_hash_table_struct *)malloc(sizeof *s);
        s->key = (char *)malloc(strlen(key) + 1);
        s->value = (char *)malloc(strlen(value) + 1);
        strcpy(s->key, key);
        strcpy(s->value, value);
        HASH_ADD_STR(hashtable, key, s); /* id: name of key field */
}

void delete_value(struct intern_hash_table_struct *s)
{
    HASH_DEL(hashtable, s);
    free(s);
}

void str_to_uint16(const char *str, uint16_t *res)
{
    char *end;
    long val = strtol(str, &end, 10);
    *res = (uint16_t)val;
}

void nachricht_weiterleiten(char nextPeerIP, char nextPeerPort, char *receive_header, char *answer_header,
                            unsigned int key_length, const char *key, const char *value)
{
    int nextPeersocket = 0;
    struct addrinfo peer_info_config;
    struct addrinfo *results;

    //initialize client_info_config
    memset(&peer_info_config, 0, sizeof(peer_info_config));
    peer_info_config.ai_protocol = IPPROTO_TCP;
    peer_info_config.ai_family = AF_INET;
    peer_info_config.ai_socktype = SOCK_STREAM;

    //set internal
    if (receive_header[0] < 128)
        answer_header[0] += 128;

    //get host information and load it into *results
    if (getaddrinfo(&nextPeerIP, &nextPeerPort, &peer_info_config, &results) != 0)

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

    send(nextPeersocket, answer_header, 1000, 0);

    //TODO Wenn internal bit 0 ist, dann muss auf 1 gesetzt werden, nachricht ggf anpassen und weiterleiten an dem nachfolger
}

void nachricht_bearbeiten(int clientsocket, char *key, unsigned int key_length, char *value, unsigned int value_length,
                          char *art, char answer_header[], int transactionId)
{
    printf("bearbeite Nachricht\n");
//    //declare needed structures
//    int fPeersocket = 0;
//    struct addrinfo peer_info_config;
//    struct addrinfo *results;
//
//    //initialize client_info_config
//    memset(&peer_info_config, 0, sizeof(peer_info_config));
//    peer_info_config.ai_protocol = IPPROTO_TCP;
//    peer_info_config.ai_family = AF_INET;
//    peer_info_config.ai_socktype = SOCK_STREAM;
//
//    //get host information and load it into *results
//    if (getaddrinfo(firstPeerIp, firstPeerPort, &peer_info_config, &results) != 0)
//    {
//        perror("getaddrinfo error");
//        exit(1);
//    }
//
//    //create new socket using data obtained with getaddrinfo()
//    fPeersocket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
//
//    //connect
//    if (connect(fPeersocket, results->ai_addr, results->ai_addrlen) == -1)
//    {
//        close(fPeersocket);
//        perror("connection error");
//        exit(1);
//    }

    if (strcmp(art, "GET") == 0)
    {
        answer_header[0] = 0b00001100;
        answer_header[1] = transactionId; // no ID
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
        struct intern_hash_table_struct *found = find_value(key);
        if (found == NULL)
        {
            // dont set value
            memcpy(&answer_header[6], key, key_length);
            send(clientsocket, answer_header, 1000, 0);
        }
        else
        {
            value = found->value;
            value_length=strlen(value);
            printf("get Key: %s, Value: %s, valuelength: %u\n",key,value, value_length);
            // set value
            memcpy(&answer_header[6], key, key_length);
            memcpy(&answer_header[6 + key_length], value, value_length);
            send(clientsocket, answer_header, 1000, 0);
        }
    }
    if (strcmp(art, "SET") == 0)
    {
        answer_header[2] = 0;
        answer_header[3] = 0;
        answer_header[4] = 0;
        answer_header[5] = 0;

        printf("set Key: %s, Value: %s\n ",key,value);

        answer_header[0] = 0b00001010;
        //answer_header[1] = transactionId; // no ID
        add_value(key, "321");
        send(clientsocket, answer_header, 1000, 0);
    }
    if (strcmp(art, "DELETE") == 0)
    {
        answer_header[2] = 0;
        answer_header[3] = 0;
        answer_header[4] = 0;
        answer_header[5] = 0;

        answer_header[0] = 0b00001001;
        answer_header[1] = transactionId; // no ID
        struct intern_hash_table_struct *found = find_value(key);
        if(found!= NULL)
        delete_value(found);
        send(clientsocket, answer_header, 1000, 0);
    }


}

void read_header_peer(char **key, char *transaktions_id, uint16_t *id_absender, char **ip_absender, char**port_absender,
                      char **value,
                      unsigned char *header)
{

    //Länge von key und value holen
    uint16_t key_length;
    uint16_t value_length;

    //Lesen länge von value und key und tauchen wir den byte order
    memcpy(&key_length, header + 2, 2);
    memcpy(&value_length, header + 4, 2);

    // key_length = ntohs(key_length);
    // value_length = ntohs(value_length);

    printf("Key length %u ", key_length);
    printf("Value length %u ", value_length);

    //Header lesen und speichern in den deklarierten Variablen, da wir schon ein pointer als parameter haben
    // übergeben das einfach in memcpy
    memcpy(transaktions_id, header + 1, 1);
    memcpy(id_absender, header + 6, 2);
    memcpy(port_absender, header + 12, 2);
    *ip_absender = malloc(4);
    memcpy(ip_absender, header + 8, 4);
    uint16_t test = ntohs(*id_absender);
    printf("T id : %s\n", transaktions_id);
    printf("Id : %u\n", *id_absender);
    printf("Ip : %s\n", *ip_absender);
    printf("Port : %s\n", *port_absender);

    //Hier Valgrind meckert wegen conditional jump or move depends on uninitialised value
    if (key_length > 0)
    {
        *key = (char *)malloc(key_length + 1);
        memcpy(*key, header + 14, key_length);
        key[key_length] = '\0';
        if (*key != NULL)
        {
            printf("Key :  %s\n", *key);
        }
    }
    else
    {
        *key = NULL;
    }
    if (value_length > 0)
    {
        *value = (char *)malloc(value_length + 1);
        memcpy(*value, header + 14 + key_length, value_length);
        value[value_length] = '\0';
        printf("Value : %s\n", *value);
    }
    else
    {
        *value = NULL;
    }
}

void read_header_client(struct peer *current_peer, char **key, char *transaktions_id, uint16_t *id_absender, char **ip_absender, char **port_absender,
                        char **value,
                        unsigned char *header, unsigned int * key_length, unsigned int * value_length)
{

    // int key_length_tem = (header[2] << 8) + header[3];   //process the key length
    // int value_length_tem = (header[4] << 8) + header[5]; //process the value length
//    for (int i= 0; i < 1000; i++) {
//        printf("%c", header[i]);
//    }
    *key_length = (header[2] << 8) + header[3]; 
    *value_length = (header[4] << 8) + header[5];

    *key = malloc(*key_length + 1);
    *value = malloc(*value_length + 1);
    *ip_absender = malloc(strlen(current_peer->current.add) + 1);
    *port_absender = malloc(strlen(current_peer->current.port) + 1);


    //Copy value and key
    memcpy(*key, &header[6], *key_length);
    memcpy(*value, &header[6 + *key_length], *value_length);

    key[*key_length] = '\0';
    value[*value_length] = '\0';

    // //Copy values in poiners
    memcpy(id_absender, &current_peer->current.id, 2);
    strcpy(*port_absender, current_peer->current.port);
    strcpy(*ip_absender, current_peer->current.add);

    printf("Id absender : %u\n", *id_absender);
    printf("Port absender %s\n", *port_absender);
    printf("Ip absender : %s\n", *ip_absender);
}

//------------------- Main

int main(int argc, char *argv[])
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

    printf("Id %u\n", current_peer->current.id);
    printf("Add %s\n", current_peer->current.add);
    printf("Port %s\n", current_peer->current.port);

    printf("Id %u\n", current_peer->vorganger.id);
    printf("Add %s\n", current_peer->vorganger.add);
    printf("Port %s\n", current_peer->vorganger.port);

    printf("Id %u\n", current_peer->nachfolger.id);
    printf("Add %s\n", current_peer->nachfolger.add);
    printf("Port %s\n", current_peer->nachfolger.port);

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
    else
    {
        printf("bind succ\n");
    }

    //prepare the socket for incoming connections
    if (listen(server_socket, 10) == -1)
    {
        perror("Error while listening");
        exit(1);
    }
    else
    {
        printf("listen succ\n");
    }
    printf("start while loop\n");

    while (1)
    {
        //construct sockaddr_storage for storing client information
        struct sockaddr_storage client_info;
        socklen_t client_info_length = sizeof(client_info);
        //accept the newest connection and load it's data to the client_info struct
        int client_socket = accept(server_socket, (struct sockaddr *)&client_info, &client_info_length);
        if (client_socket == -1)
        {
            perror("Error while accepting the connection");
            exit(1);
        }
        else
        {
            printf("client accepted\n");
        }

        unsigned char receive_header[1000];
        unsigned char answer_header[1000];

        if (recv(client_socket, &receive_header, 1000, 0) == -1) //receive header
        {
            perror("Error receiving");
            exit(1);
        }
        else
        {
            printf("recv success\n");
        }

        int internal = isNthBitSet(receive_header[0], 1);

        char *art;
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

        //Wenn es intern ist, dann lesen wir read_header_peer, andersfalls lesen wir read_header_client

        unsigned int key_length;
        unsigned int value_length;
        char *key;
        char transaktions_id;
        uint16_t id_absender;
        char* port_absender;
        char *ip_absender;
        char *value;

        if (internal)
        {
            // //TODO Header lesen. Wir müssen anders als vorherige aufgabe machen
            printf("read header peer...\n");
            read_header_peer(&key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value, answer_header);
        }
        else
        {
            printf("read header client...\n");
        read_header_client(current_peer, &key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value, receive_header, &key_length, &value_length);            
    //TODO key_length, value_length missing and ip_absender, port_absender in char format
            nachricht_bearbeiten(client_socket, key, key_length, value, value_length, art, answer_header, transaktions_id);
        }

        //nachricht_ausgeben(current_peer, internal, art, id_absender, ip_absender, port_absender);

        // //Hash der Key und gucke ob das zu dem aktuellen peer gehört
        // printf("hashs key: %s with length+1: %u\n", key, key_length+1);
        // unsigned int hash_value = hash(key, (key_length+1)); //TODO read header before that

        /*
        //Prüfen ob den aktuellen peer zuständig für die anfrage ist
        //Wenn ja, bearbeite die anfrage (also führe set, get, delete in interne hastable aus)
        //Wenn nicht setzte das intern bit zu 1 und leite dass zu nachfolger weiter

        // 2 Fälle
        // Fall 1 : hash liegt zwischen den letzen und dem ersten Knoten
        // Bsp letzte knoten 220 und ersten 10
        // Fall 2 : hash liegt zwischen zwei normalen knoten.


        //Fall 1 (ersten Knoten): wir gucken ob der current peer ein kleineres ID hat als vorgänger, wenn ja es ist Fall 1 ansonsten Fall 2

        if(atoi(current_peer->current.id) < atoi(current_peer->vorganger.id)){
            if(hash_value < atoi(current_peer->current.id) || hash_value > atoi(current_peer->vorganger.id)){
                //Dann current ist dafür zuständig

                //TODO Nachricht bearbeiten und an absender zurückschicken
                nachricht_bearbeiten(client_socket, receive_header, answer_header, key_length, key, value);

            }else{
                //Nicht zuständig, dann weiter leiten

                //TODO Nachricht weiterleiten
                nachricht_weiterleiten(client_socket, internal ,receive_header, answer_header, key_length, key, value);
            }

        }else{
            //Fall 2: also normalen Fall
            if(hash_value > atoi(current_peer->current.id) || hash_value < atoi(current_peer->vorganger.id)){
                // Nachricht weiterleiten
            }else{
                // Nachricht bearbeiten
            }
        }*/
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}

