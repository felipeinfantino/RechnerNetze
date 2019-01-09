#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>

void read_response(){

}



int main(int argc, char *argv[]) {

    if(argc != 3){ //TODO
        perror("wrong number of arguments");
        exit(1);
    }
    //Localen socket erstellen
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    //Port und ipOrDNS holen //TODO
    char *ipOrDNS = argv[1];
    char *portChar = argv[2];

    struct timespec start, stop;
    double time;

    struct addrinfo socket_to_conect_config;
    memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config)); //Setzen die zu null
    socket_to_conect_config.ai_family = AF_INET;
    socket_to_conect_config.ai_socktype =SOCK_DGRAM;

    struct addrinfo* results;
    int result = getaddrinfo(ipOrDNS, portChar, &socket_to_conect_config, &results);

    if(result == -1){
        perror("Cannot resolve host or wrong port");
        exit(1);
    }

    //TODO
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
    printf("%s",response_from_server );

    if (clock_gettime(CLOCK_REALTIME, &timevariable) == -1) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

    close(client_socket);
    freeaddrinfo(results);
    return 0;
}

