#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <bits/time.h>
#include <time.h>


void append(char *s, char c) {
    int len = strlen(s);
    s[len] = c;
    s[len + 1] = '\0';

}


int check_if_is_http(char *ipOrDNSOrHttp) {
    char *http = "http://";
    int leng = strlen(http);
    int is_http = strlen(ipOrDNSOrHttp) > leng;
    for (int i = 0; i < leng && is_http; i++) {
        if (http[i] != ipOrDNSOrHttp[i]) {
            return 0;
        }
    }
    return 1;
}


void split_domain(char *httpLink, char *domain_buffer, char *pfad_buffer, char *http_request_buffer) {
    //start on 7 because of http://
    for (int i = 7; i < strlen(httpLink); i++) {
        if (httpLink[i] == '/') {
            for (int p = i; p < strlen(httpLink); p++) {
                append(pfad_buffer, httpLink[p]);
            }
            break;
        }
        append(domain_buffer, httpLink[i]);
    }
    strcat(http_request_buffer, "GET ");
    strcat(http_request_buffer, pfad_buffer);
    strcat(http_request_buffer, " HTTP/1.1\r\nHOST:");
    strcat(http_request_buffer, domain_buffer);
    strcat(http_request_buffer, "\r\n\r\n");
}


int main(int argc, char *argv[]) {
    struct timespec start, stop;
    double time;
    int i=5;

    if (argc != 2) {
        fprintf(stderr, "%i Argumente übergeben, Erwartet nur das HTTP link", (argc - 1));
        perror("");
        exit(1);
    }

    //start timer
    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

    //Localen socket erstellen
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    //Port und HttpLink holen
    char *HttpLink = argv[1];

    //Check if there given argument is an http link
    int is_http = check_if_is_http(HttpLink);

    char domain_buffer[256] = "";
    char pfad_buffer[256] = "";
    char http_request_buffer[256] = "";

    if (is_http) {
        split_domain(HttpLink, domain_buffer, pfad_buffer, http_request_buffer);
    }

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
    int result = getaddrinfo(domain_buffer, "80", &socket_to_conect_config, &connection_results);

    if (result != 0) {
        perror("Ungültige Eingaben, prüfen sie die Eingaben");
        exit(1);
    }

    //Wir nehmen das erste element von dem results und seine Adresse
    struct sockaddr *first_result = connection_results->ai_addr;
    socklen_t first_result_addr_len = connection_results->ai_addrlen;

    //Verbindung zwischen client_socket, mit dem first_result, und wir übergeben die first_result_addr_len
    int connection = connect(client_socket, first_result, first_result_addr_len);
    if (connection == -1) {
        perror("Fehler bei der Verbindung");
        exit(1);
    }


    if (is_http) {
        int bytes_sent = send(client_socket, http_request_buffer, strlen(http_request_buffer), 0);
        if (bytes_sent == -1) {
            perror("Error while sending http Request");
            exit(1);
        }
    }


    char response_from_server[1000000];
    int receive = recv(client_socket, &response_from_server, 1000000, 0);
    char *receive_ohne_header;
    receive_ohne_header = strstr(response_from_server, "\r\n\r\n");
    if(receive_ohne_header!=NULL)
    printf("%s",receive_ohne_header+4);
    while(recv(client_socket, &response_from_server, 1000000, 0)>0){
    printf("%s",response_from_server);
    }

    /*while(i>0){
    if (receive == -1) {
        perror("Error while receiving data");
        exit(1);
    }


    //ACHTUNG NUL CHAR ÜBERPÜFUNG FEHLT
   

    printf("%s",response_from_server);
    i--;
    receive = recv(client_socket, &response_from_server, 200000, 0);
    }
*/
    //End timer
    /*clock_gettime(id, &tp);
    __syscall_slong_t end_time = tp.tv_nsec;


    //Difference in nano seconds
    printf("Difference %ld ns", end_time-start_time);
    */
    if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }
    //check if more than 1 passed and correct nanoseconds
    struct timespec tmpTime;
    if ((stop.tv_nsec - start.tv_nsec) < 0) {
        tmpTime.tv_nsec = 1000000000 + stop.tv_nsec - start.tv_nsec; //add 1s in nanoseconds
    } else {
        tmpTime.tv_nsec = stop.tv_nsec - start.tv_nsec;
    }
    time = tmpTime.tv_nsec / 1000000; //convert to ms
    //printf("%lf ms\n", time);

    close(client_socket);
    //Free
    freeaddrinfo(connection_results);
    return 0;
}