
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

char* get_random_zeile(const char *filename,FILE *file) {
	char* line = malloc(sizeof(char)*512);
	
    //Wir zählen die Anzahl von Zahlen
	int anzahl_zeilen = 0;
	while(!feof(file)){
		fgets(line, 512, file);
		anzahl_zeilen++;
	}

	//Wir berechen eine random Zahl und geben die Zeile zurück
    //Initialisierung von rand()
	srand(time(NULL));
	int random_zeile = rand() % anzahl_zeilen;
	//Reset File Positon
	fseek (file, 0, SEEK_SET);
	for(int i=0;i<anzahl_zeilen;i++){
		fgets(line, 512, file);
		if(i == random_zeile){
			fclose(file);
			return line;
		}
	}

}


int main(int argc, char* argv[]){

    //Wir holen die argumente, prüfen dass es genau argumente übergegeben wurden.
	if(argc != 3){
		fprintf(stderr, "%i Argumente übergeben, Erwartet nur 2, nähmlich IP oder DNS und Port", (argc-1));
		perror("");
		exit(1);
	}

    //Port und filename
	char *port = argv[1];
	const char *filename = argv[2];

	//Check whether file exists and open it
	FILE *file = fopen(filename, "rt");
	if(file == NULL){
		perror("Error");
		exit(1);
	}

	//Check if file is empty
	struct stat fileStat;
	stat(filename, &fileStat);

	if(fileStat.st_size ==0){
		printf("File is empty\n");
		return(0);
	}

	struct addrinfo server_info_config;
	struct addrinfo* results;

    memset(&server_info_config, 0, sizeof(server_info_config)); //Initialisieren mit NULL
    //Befüllen die notwendige Einträge
    server_info_config.ai_family = AF_INET;
    server_info_config.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo("localhost", port , &server_info_config, &results);

    //Socket erzeugen, domain ist AF_INET, type ist SOCK_STREAM und Prokoll ist 0, wir lesen das aber von dem results
    int server_socket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //Wir rufen bind auf, somit weisen wir eine adresse zu dem socket zu. Die Adresse holen wir uns von dem results
    int erfolgreiche_bind = bind(server_socket, results->ai_addr, results->ai_addrlen);

    if(erfolgreiche_bind == -1){
    	perror("Konnte nicht die Adresse zu dem Socket zuweisen");
    	exit(1);
    }

    //Nach dem bind wir fangen an, requests zu erwarten mit listen()
    int erfolgreiche_listen = listen(server_socket, 10);

    if(erfolgreiche_listen == -1){
    	perror("Fehler beim listen()");
    	exit(1);
    }

    //Dann erstellen wir ein sockaddr_storage, wo wir die Info von Client speichern werden , und natürlich den client_socket
    struct sockaddr_storage client_info;
    socklen_t client_info_length = sizeof(client_info); // Die größe von dem client info struct

    int client_socket = accept(server_socket, (struct sockaddr *) &client_info, &client_info_length);

    //So jetzt das wir den client_socket haben, wir nehmen eine random Zeile von der übergegebene File und senden das
    char *random_zeile = get_random_zeile(filename,file);
    size_t len_from_zeile = strlen(random_zeile);

    if(random_zeile == NULL){
    	perror("Fehler beim lesen der Datei");
    	exit(1);
    }

    //Senden
    ssize_t bytes_sent = send(client_socket, random_zeile, len_from_zeile,0);
    if(bytes_sent == -1){
    	perror("Fehler beim senden");
    	exit(1);
    }

    //close sockets
    close(client_socket);
    close(server_socket);

    //free den addrinfo
    freeaddrinfo(results);
}

