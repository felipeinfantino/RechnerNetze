#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {

    if(argc != 3){
        fprintf(stderr, "%i Argumente übergeben, Erwartet nur 2, nähmlich IP oder DNS und Port", (argc-1));
        perror("");
        exit(1);
    }
    //Localen socket erstellen
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    //Port und ipOrDNS holen
    char *ipOrDNS = argv[1];
    char *portChar = argv[2];

    struct addrinfo socket_to_conect_config;
    memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config)); //Setzen die zu null
    socket_to_conect_config.ai_family = AF_INET;
    socket_to_conect_config.ai_socktype =SOCK_DGRAM;


    //Start timer
    clockid_t id = CLOCK_MONOTONIC;
    struct timespec tp;
    clock_gettime(id, &tp);
    __syscall_slong_t start_time = tp.tv_nsec;

    struct addrinfo* results;
    int result = getaddrinfo(ipOrDNS, portChar, &socket_to_conect_config, &results);

    if(result == -1){
        perror("Cannot resolve host or wrong port");
        exit(1);
    }

    ssize_t successful_sent = sendto(client_socket, "hola", sizeof(char)*4,0, results->ai_addr, results->ai_addrlen);

    if(successful_sent == -1){
        perror("Error while sending the data");
        exit(1);
    }

    char response_from_server[512];
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len=sizeof(src_addr);

    ssize_t receive = recvfrom(client_socket, &response_from_server, sizeof(response_from_server)+1, 0, (struct sockaddr*)&src_addr,&src_addr_len);

    if(receive == -1){
        perror("Fehler bei der Datenuberstragung");
        exit(1);
    }
    //End timer
    clock_gettime(id, &tp);
    __syscall_slong_t end_time = tp.tv_nsec;
    //Difference in nano seconds
    printf("Response : %s\n", response_from_server);
    printf("Difference %ld ns", end_time-start_time);


    close(client_socket);
    //shutdown(client_socket, SHUT_RDWR);
    //Free
    freeaddrinfo(results);
    return 0;
}

