#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

char* get_random_quote(FILE *file) {
	char* line = malloc(sizeof(char)*512);
	
    //count the number of quotes
	int number_of_quotes = 0;
	while(!feof(file)){
		fgets(line, 512, file);
		number_of_quotes++;
	}

    //use both time and PID to generate seed, else server is stuck in returning same quotes
	srand(time(0)+getpid());
	int random_quote = rand() % number_of_quotes;

	//prepare file position for selecting quote
	fseek (file, 0, SEEK_SET);
	for(int i=0;i<number_of_quotes;i++){
		fgets(line, 512, file);
		if(i == random_quote){
			return line;
		}
	}

}


int main(int argc, char* argv[]){

    //check for correct number of arguments
	if(argc != 3){
		fprintf(stderr, "%i arguments inputed, 3 are needed for correct program execution", (argc-1));
		perror("");
		exit(1);
	}

    //port and filename
	char *port = argv[1];
	char *filename = argv[2];

	//check whether file exists
	FILE *file = fopen(filename, "rt");
	if(file == NULL){
		perror("Error");
		exit(1);
	}

	//check if file is empty
	struct stat fileStat;
	stat(filename, &fileStat);

	if(fileStat.st_size ==0){
		printf("File is empty\n");
		return(0);
	}

    //declare needed structures
	struct addrinfo server_info_config;
	struct addrinfo* results;

    //initialize server_info_config 
    memset(&server_info_config, 0, sizeof(server_info_config)); 
    server_info_config.ai_family = AF_INET;
    server_info_config.ai_socktype = SOCK_STREAM;

    //get host information and load it into *results
    if(getaddrinfo("localhost", port , &server_info_config, &results)!=0){
        perror("");
        exit(1);
    }

    //create new socket using data obtained with getaddrinfo()
    int server_socket = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    //bind the new socket to a port
    if(bind(server_socket, results->ai_addr, results->ai_addrlen)==-1){
        perror("Couldn't bind the socket to this port");
        exit(1);
    }

    //prepare the socket for incoming connections
    if(listen(server_socket, 10)==-1){
       perror("Error while listening");
       exit(1);
   }

    //start an infinite loop which monitors for new connections and handles it (the ~server~ program)
   while(1){
    //construct sockaddr_storage for storing client information
    struct sockaddr_storage client_info;
    socklen_t client_info_length = sizeof(client_info); 

    //aceept the newest connection and load it's data to the client_info struct
    int client_socket = accept(server_socket, (struct sockaddr *) &client_info, &client_info_length);
    if (client_socket == -1) {
        perror("Error while accepting the connection");
        exit(1);
    }

    //fork a new process to handle this client 
    if(!fork()){    //child process
        //fetch a random quote from the TXT
        char *random_quote = get_random_quote(file);
        size_t quote_length = strlen(random_quote);
        //close the "listener" socket in child process - no need for child to wait monitor for new incomming connections
        close(server_socket);
        //send a random quote to the client
        send(client_socket, random_quote, quote_length+1,0);
        printf("Sent quote: %s", random_quote );
        //after sending the quote, close the socket in child process
        close(client_socket);
        exit(0);
    }
   //this client has been already handled by the server, close this connection
    close(client_socket);
}
//clear the memory and close the file stream. this server doesn't support "safe quitting" though, needs to be killed by the OS
freeaddrinfo(results);
fclose(file);
}
