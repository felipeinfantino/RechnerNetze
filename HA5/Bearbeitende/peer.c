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
    uint16_t id;
    char* add;
    uint16_t port;
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


void read_header(char **pString, char *string, uint16_t *pInt, uint32_t *pInt1, uint16_t *pInt2, char **pString1,
                 unsigned char header[1000]);

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
void nachricht_ausgeben(struct peer* current_peer, int internal, char* art, uint16_t id_absender, uint32_t ip_absender, uint16_t port_absender){
    printf("Internal : %i\n", internal);
    printf("Art der Nachricht : %s\n", art);
    printf("Absender : \n\t ID : %u\n\t IP : %u\n\t PORT : %u\n\n", id_absender, ip_absender, port_absender);
    printf("Vorgänger : \n\t ID : %s\n\t IP : %s\n\t PORT : %s\n\n", current_peer->vorganger.id, current_peer->vorganger.add, current_peer->vorganger.port);
    printf("Nachfolger : \n\t ID : %s\n\t IP : %s\n\t PORT : %s\n\n", current_peer->nachfolger.id, current_peer->nachfolger.add, current_peer->nachfolger.port);
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

void str_to_uint16(const char *str, uint16_t *res) {
    char *end;
    long val = strtol(str, &end, 10);
    *res = (uint16_t)val;
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
    //Initalize peer
    struct peer* current_peer = malloc(sizeof(current_peer));

    //initialisiere structs
    current_peer->current = (struct id_add_port){.id= 0, .add = malloc(strlen(argv[2])) , .port=0};
    current_peer->vorganger = (struct id_add_port){.id= 0, .add = malloc(strlen(argv[5])) , .port=0};
    current_peer->nachfolger = (struct id_add_port){.id= 0, .add = malloc(strlen(argv[8])) , .port= 0};

    //Kopiere eingaben in den structs, bei id und port sind ja uint16 und add is string
    //current
    str_to_uint16(argv[1], &current_peer->current.id);
    strncpy(current_peer->current.add, argv[2], strlen(argv[2]));
    str_to_uint16(argv[3], &current_peer->current.port);


    //vorganger
    str_to_uint16(argv[4], &current_peer->vorganger.id);
    strncpy(current_peer->vorganger.add, argv[5], strlen(argv[5]));

    //str_to_uint16(argv[6], &current_peer->vorganger.port);




    printf("Id %u\n",current_peer->current.id);
    printf("Add %s\n",current_peer->current.add);
    printf("Port %u\n",current_peer->current.port);

    /*
    printf("Id %u\n",current_peer->vorganger.id);
    printf("Add %s\n",current_peer->vorganger.add);
    printf("Port %u\n",current_peer->vorganger.port);

    printf("Id %u\n",current_peer->nachfolger.id);
    printf("Add %s\n",current_peer->nachfolger.add);
    printf("Port %u\n",current_peer->nachfolger.port);

*/


    /*
     *


    if(current_peer->current.id == 0 || current_peer->vorganger.id == 0 || current_peer->nachfolger.id == 0){
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
        char* key; //LESEN WENN MAN KEYLENGHT HAT
        char transaktions_id;
        uint16_t id_absender;
        uint16_t key_length;
        uint16_t port_absender;
        uint32_t ip_absender;
        char* value = "TO_READ_FROM_HEADER";

        read_header(&key, &transaktions_id, &id_absender, &ip_absender, &port_absender, &value,receive_header);


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
        }
        close(client_socket);
    }
    close(server_socket);
    freeaddrinfo(results);




    */
}

void read_header(char **key, char *transaktions_id, uint16_t *id_absender, uint32_t *ip_absender, uint16_t *port_absender, char **value,
                 unsigned char* header) {

    //Länge von key und value holen
    uint32_t key_length;
    uint32_t value_length;

    //Lesen länge von value und key und tauchen wir den byte order
    memcpy(&key_length, header+2, 2);
    memcpy(&value_length, header+4, 2);

    key_length = ntohl(key_length);
    value_length = ntohl(key_length);

    //Header lesen und speichern in den deklarierten Variablen, da wir schon ein pointer als parameter haben
    // übergeben das einfach in memcpy
    memcpy(transaktions_id, header+1, 1);
    memcpy(id_absender, header+6, 2);
    memcpy(ip_absender, header+8, 4);
    memcpy(port_absender, header+10, 2);

    if(key_length > 0){
        *key = (char*)malloc(key_length +1);
        memcpy(*key, header+14, key_length);
    }else{
        *key = NULL;
    }
    if(value_length > 0){
        *value = (char*)malloc(value_length +1);
        memcpy(*key, header+14+key_length, value_length);
    } else{
        *value = NULL;
    }
    //TODO vielleicht die nullterminierende chars nach memcpy
}



