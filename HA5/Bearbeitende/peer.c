#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "uthash.h"

#define HEAD 6


//------------------- Structs
struct id_add_port{
    char* id;
    char* add;
    char* port;
};


struct peer{
    struct id_add_port current;
    struct id_add_port vorganger;
    struct id_add_port nachfolger;
};

struct intern_hash_table_struct {
    char *key;
    char *value;
    UT_hash_handle hh;
};

struct intern_hash_table_struct *hashtable = NULL;

//------------------- Funktionen die Structs zurückgeben

struct intern_hash_table_struct *find_value(char key[]) {
    struct intern_hash_table_struct *s;
    HASH_FIND_STR(hashtable, key, s );
    return s;
}

//------------------- Funktionen die int zurückgeben

int isNthBitSet (unsigned char c, int n) {
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}


unsigned int hash (const char *str, unsigned int length) {
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
void nachricht_ausgeben(struct peer current_peer, int internal, char* art,char*id_absender, char*ip_absender, char* port_absender){
    printf("Internal : %i\n", internal);
    printf("Art der Nachricht : %s\n", art);
    printf("Absender : \n\t ID : %s\n\t IP : %s\n\t PORT : %s\n\n", id_absender, ip_absender, port_absender);
    printf("Vorgänger : \n\t ID : %s\n\t IP : %s\n\t PORT : %s\n\n", current_peer.vorganger.id, current_peer.vorganger.add, current_peer.vorganger.port);
    printf("Nachfolger : \n\t ID : %s\n\t IP : %s\n\t PORT : %s\n\n", current_peer.nachfolger.id, current_peer.nachfolger.add, current_peer.nachfolger.port);
}




void nachricht_bearbeiten(int client_socket, unsigned char receive_header[], unsigned char answer_header[],
                          unsigned int key_length, const char *key, const char *value) {
    //TODO muss angepasst werden , internal bit dann wieder zu 0 , denn der client wird das empfangen
    //Antoworten Format soll so aussehen
    // antwort[0] = wie in Aufgaben blatt mit dem internal als 0, ack 1 und den entsprechen type auf 1 (also GET, SET, DELETE)
    // antwort[1] = transaktionsID
    // antwort[2-5] = IP von dem Knoten der das bearbeitet hat
    // antwort[6-7] = Port von dem Knoten der das bearbeitet hat
    if (receive_header[0] == 4) //GET
    {
        struct intern_hash_table_struct *found = find_value(key);
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
        struct intern_hash_table_struct *toDel = (struct intern_hash_table_struct *)malloc(sizeof *toDel);
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
}

void nachricht_weiterleiten(int client_socket, int internal ,unsigned char receive_header[], unsigned char answer_header[],
                          unsigned int key_length, const char *key, const char *value) {
    //TODO Wenn internal bit 0 ist, dann muss auf 1 gesetzt werden, nachricht ggf anpassen und weiterleiten an dem nachfolger
}


void add_value(char key[], char value[]) {
    struct intern_hash_table_struct *s = NULL;
    HASH_FIND_STR(hashtable, key, s);  /* id already in the hash? */
    if (s==NULL) {
        s = (struct intern_hash_table_struct *)malloc(sizeof *s);
        s->key = (char *) malloc(strlen(key) + 1);
        s->value = (char *) malloc(strlen(value) + 1);
        strncpy(s->key, key, strlen(key));
        strncpy(s->value, value, strlen(value));
        HASH_ADD_STR(hashtable, key, s);  /* id: name of key field */
    }
}




void delete_value(struct intern_hash_table_struct *s) {
    HASH_DEL(hashtable, s);
    free(s);
}




//------------------- Main

int main(int argc, char *argv[])
{
    //check for correct number of arguments
    if (argc != 10)
    {
        perror("Wrong input format");
        exit(1);
    }
    //Bsp aufruf ./peer 15 127.0.0.1 4711 245 127.0.0.1 4710 112 127.0.0.1 4712
    //TODO checken dass keine ID 0 ist
    //Initalize peer
    struct peer current_peer;
    strncpy(current_peer.current.id, argv[1], strlen(argv[1]));
    strncpy(current_peer.current.add, argv[2], strlen(argv[2]));
    strncpy(current_peer.current.port, argv[3], strlen(argv[3]));

    strncpy(current_peer.vorganger.id, argv[4], strlen(argv[4]));
    strncpy(current_peer.vorganger.add, argv[5], strlen(argv[5]));
    strncpy(current_peer.vorganger.port, argv[6], strlen(argv[6]));

    strncpy(current_peer.nachfolger.id, argv[7], strlen(argv[7]));
    strncpy(current_peer.nachfolger.add, argv[8], strlen(argv[8]));
    strncpy(current_peer.nachfolger.port, argv[9], strlen(argv[9]));

    //declare needed structures
    struct addrinfo server_info_config;
    struct addrinfo *results;

    //initialize server_info_config
    memset(&server_info_config, 0, sizeof(server_info_config));
    server_info_config.ai_protocol = IPPROTO_TCP;
    server_info_config.ai_family = AF_INET;
    server_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if (getaddrinfo(current_peer.current.add, current_peer.current.port, &server_info_config, &results) != 0)
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

        // TODO klären ob den Key tatsächlcich optional ist, und wenn key nicht vorhanden was machen wir dann?
        //Nach dem wir die Nachricht empfangen haben, hashen wir den Key
        //TODO Header lesen. Wir müssen anders als vorherige aufgabe machen
        unsigned int key_length = 0; //LESEN
        char* key = "TO_READ_FROM_HEADER";//LESEN WENN MAN KEYLENGHT HAT
        char* transaktions_id = "TO_READ_FROM_HEADER";
        char* id_absender = "TO_READ_FROM_HEADER";
        char* ip_absender = "TO_READ_FROM_HEADER";
        char* port_absender = "TO_READ_FROM_HEADER";
        char* value = "TO_READ_FROM_HEADER";


        //Bereite die ausgabe vor
        int internal = isNthBitSet(receive_header[0], 1);

        char* art;
        int isGet = isNthBitSet(receive_header[0], 5);
        int isSet = isNthBitSet(receive_header[0], 6);
        int isDelete = isNthBitSet(receive_header[0], 7);

        if(isGet) strcpy(art, "GET");
        if(isSet) strcpy(art, "SET");
        if(isDelete) strcpy(art, "DELETE");

        if(art == NULL){
            perror("Wrong usage, only set or get or delete.");
            exit(1);
        }

        nachricht_ausgeben(current_peer, internal, art, id_absender, ip_absender, port_absender);

        //Hash der Key und gucke ob das zu dem aktuellen peer gehört
        unsigned int hash_value = hash(key, (key_length+1));

        //Prüfen ob den aktuellen peer zuständig für die anfrage ist
        //Wenn ja, bearbeite die anfrage (also führe set, get, delete in interne hastable aus)
        //Wenn nicht setzte das intern bit zu 1 und leite dass zu nachfolger weiter

        // 2 Fälle
        // Fall 1 : hash liegt zwischen den letzen und dem ersten Knoten
        // Bsp letzte knoten 220 und ersten 10
        // Fall 2 : hash liegt zwischen zwei normalen knoten.


        //Fall 1 (ersten Knoten): wir gucken ob der current peer ein kleineres ID hat als vorgänger, wenn ja es ist Fall 1 ansonsten Fall 2

        if(atoi(current_peer.current.id) < atoi(current_peer.vorganger.id)){
            if(hash_value < atoi(current_peer.current.id) || hash_value > atoi(current_peer.vorganger.id)){
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
            if(hash_value > atoi(current_peer.current.id) || hash_value < atoi(current_peer.vorganger.id)){
                // Nachricht weiterleiten
            }else{
                // Nachricht bearbeiten
            }
        }
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);
}

