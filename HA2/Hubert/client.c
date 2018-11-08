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
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

        //Port und ipOrDNS holen
    char *ipOrDNS = argv[1];
    char *portChar = argv[2];

        //Prüfen, dass der port nummer Nur zahlen enthält
    size_t len;
    len = strlen(portChar);
    for(int i=0;i<len;i++)
    {
        if(portChar[i] < 48 || portChar[i] > 57)
        {
            perror("Not a valid port number");
            exit(1);
        }
    }

    struct timespec start;
    struct timespec end;
    clockid_t clk_id=CLOCK_MONOTONIC;

        //get start clock
    if(clock_gettime(clk_id,&start)==-1){
        perror("Error setting clock");
        exit(1);
    }

        struct addrinfo socket_to_conect_config; //Minimale Konfiguration für die Verbindung
        memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config)); //Setzen die zu null
        socket_to_conect_config.ai_family = AF_INET;
        socket_to_conect_config.ai_socktype = SOCK_STREAM;

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
        int receive = recv(client_socket, &response_from_server, sizeof(response_from_server), 0);
        while(receive != 0){
            if(receive == -1){
                perror("Fehler bei der Datenuberstragung");
                exit(1);
            }
            printf("%s", response_from_server);
            receive = recv(client_socket, &response_from_server, sizeof(response_from_server), 0);
        }
        
        close(client_socket);

        //Free
        freeaddrinfo(connection_results);

        //get end clock
        if(clock_gettime(clk_id,&end)==-1){
            perror("Error setting clock");
            exit(1);
        }

        //calculate runtime
        int runtime=1000000000*(end.tv_sec-start.tv_sec)+end.tv_nsec-start.tv_nsec;
        //display runtime in miliseconds
        printf("\n Runtime: %dms \n",runtime/1000000);
        


        return 0;
    }