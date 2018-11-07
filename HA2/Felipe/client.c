#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <bits/time.h>
#include <time.h>


int main(int argc, char *argv[]) {

    if(argc != 3){
        fprintf(stderr, "%i Argumente übergeben, Erwartet nur 2, nähmlich IP oder DNS und Port", (argc-1));
        perror("");
        exit(1);
    }
    //Localen socket erstellen
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    //Port und ipOrDNS holen
    char *ipOrDNS = argv[1];
    char *portChar = argv[2];

    struct addrinfo socket_to_conect_config; //Minimale Konfiguration für die Verbindung
    memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config)); //Setzen die zu null
    socket_to_conect_config.ai_family = AF_INET;
    socket_to_conect_config.ai_socktype = SOCK_STREAM;

    //Start timer
    clockid_t id = CLOCK_MONOTONIC;
    struct timespec tp;
    clock_gettime(id, &tp);
    __syscall_slong_t start_time = tp.tv_nsec;

    struct addrinfo *connection_results;
    int result = getaddrinfo(ipOrDNS, portChar, &socket_to_conect_config, &connection_results);

    if(result != 0){
        perror("Ungültige Eingaben, prüfen sie die Eingaben");
        exit(1);
    }

    //Wir nehmen das erste element von dem results und seine Adresse
    struct sockaddr *first_result = connection_results->ai_addr;
    socklen_t first_result_addr_len = connection_results->ai_addrlen;

    //Verbindung zwischen client_socket, mit dem first_result, und wir übergeben die first_result_addr_len
    int connection = connect(client_socket, first_result, first_result_addr_len);
    if(connection == -1){
        perror("Fehler bei der Verbindung");
        exit(1);
    }

    char response_from_server[512];
    ssize_t receive = recv(client_socket, &response_from_server, sizeof(response_from_server)+1, 0);
    if(receive == -1){
        perror("Fehler bei der Datenuberstragung");
        exit(1);
    }

    //End timer
    clock_gettime(id, &tp);
    __syscall_slong_t end_time = tp.tv_nsec;

    //Difference in nano seconds
    printf("Difference %ld ns", end_time-start_time);

    close(client_socket);

    //Free
    freeaddrinfo(connection_results);
    return 0;
}