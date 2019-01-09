#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>

#define SIZE 48
#define UDP "123"
#define UNIX_TIME_OFFSET 2208988800

typedef struct message
{
    unsigned char header;
    unsigned char stratum;
    unsigned char poll;
    unsigned char precision;
    unsigned int delay;
    unsigned int dispersion;
    unsigned int reference_id;
    unsigned int reference[2];
    unsigned int origin[2];
    unsigned int receive[2];
    unsigned int transmit[2];
} message;

void read_response()
{
}

int compute_delay() {}

int compute_offset() {}

int compute_dispersion() {}

/*
Geben Sie fur jeden Server folgende Informationen aus (Zeitangabe in Sekunden mit min. 6 ¨
Nachkommastellen):
fIP oder Hostnameg fRoot Dispersion (Avg.)g fDispersiong fDelay (Avg.)g
fOffset(Avg.)g

*/
void print_information() {}

/*
Geben Sie anschließend den gewahlten Server aus, die lokale Zeit, den durchschnittlichen Offset ¨
und die angepasste Zeit aus.
*/
void compute_best_server() {}

int main(int argc, char *argv[])
{

    //copy servers from console

    char *server = argv[1];
    //for (int i = 0; i < number_of_servers; i++)
    //{
    message *msg;
    //SET First byte according to protocol
    msg = calloc(sizeof(message), SIZE);
    msg->header = 0b00100011;

    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    struct addrinfo socket_to_conect_config;
    memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config));
    socket_to_conect_config.ai_family = AF_INET;
    socket_to_conect_config.ai_socktype = SOCK_DGRAM;

    struct addrinfo *results;
    int result = getaddrinfo(server, UDP, &socket_to_conect_config, &results);

    if (result == -1)
    {
        perror("getaddrinfo error");
        exit(1);
    }

    //TODO
    /*for (int i = 0; i < 8; i++)
    {
        //get system time
        if (clock_gettime(CLOCK_REALTIME, &origin_time[i]) == -1)
        {
            perror("clock gettime");
            exit(1);
        }
      */
    //convert time according to
    printf("%ld\n", sendto(client_socket, msg, SIZE, 0, results->ai_addr, results->ai_addrlen));
    char *response;
    //for (int i = 0; i < 8; i++)

    response = malloc(SIZE + 1);

    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    //for (int i = 0; i < 8; i++)
    //{
    printf("%ld\n", recvfrom(client_socket, &response, SIZE + 1, 0, (struct sockaddr *)&src_addr, &src_addr_len));

    //parse the message
    /*
    msg->poll = response + 2;
    msg->precision = response + 3;
    msg->delay = response + 4;
    msg->dispersion = response + 8;
    msg->reference_id = response + 12;
    msg->reference[0] = response + 16;
    msg->reference[1] = response + 20;
    msg->origin[0] = response + 24;
    msg->origin[1] = response + 28;
    msg->receive[0] = response + 32;
    msg->receive[1] = response + 36;
    msg->transmit[0] = response + 40;
    msg->transmit[1] = response + 44;
    //message parsed
*/
    //}

    //print_information();

    close(client_socket);
    freeaddrinfo(results);
}
