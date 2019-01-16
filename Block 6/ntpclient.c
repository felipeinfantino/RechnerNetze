#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>

#define SIZE 48
#define UDP "123"
#define UNIX_TIME_OFFSET 2208988800

//Macros taken from RFC 5905
typedef unsigned long long tstamp;         /* NTP timestamp format */
typedef unsigned int tdist;                /* NTP short format */
#define FRIC 65536.                        /* 2^16 as a double */
#define D2FP(r) ((unsgined int)((r)*FRIC)) /* NTP short */
#define FP2D(r) ((double)(r) / FRIC)
#define JAN_1970 2208988800UL
#define FRAC 4294967296.              /* 2^32 as a double */
#define D2LFP(a) ((tstamp)((a)*FRAC)) /* NTP timestamp */
#define LFP2D(a) ((double)(a) / FRAC)
#define U2LFP(a) (((unsigned long long)((a).tv_sec + JAN_1970) << 32) + (unsigned long long)((a).tv_usec / 1e6 * FRAC))
//END OF MACROS

typedef struct message
{
    unsigned char header;
    unsigned char stratum;
    unsigned char poll;
    char precision; //signed, according to RFC
    tdist delay;
    tdist dispersion;
    int reference_id; // 32bit char, according to RFC
    tstamp reference;
    tstamp origin;
    tstamp receive;
    tstamp transmit;
} message;

//Function modified from RFC 5905
tstamp get_time()
{
    tstamp current_time;
    unsigned int fractions;
    unsigned int seconds;
    struct timeval unix_time;
    gettimeofday(&unix_time, NULL);
    seconds = unix_time.tv_sec + UNIX_TIME_OFFSET;
    fractions = unix_time.tv_usec / 1e6 * FRAC;
    current_time = (((tstamp)seconds) << 32) | ((tstamp)fractions);
    return current_time;
}

/*
*   Function to read chars from header and convert them to NTP timestamp format
*   Input: char header, starting position of time to convert
*   Output: Time in tstamp format
*/

tstamp char_to_tstamp(unsigned char *header, int index)
{
    tstamp lfp = 0;
    unsigned int seconds = 0;
    unsigned int fractions = 0;
    for (int i = 0; i < 8; i++)
    {
        if (i < 4)
            seconds += (header[index + i] << (24 - 8 * i));
        else
            fractions += (header[index + i] << (24 - 8 * (i - 4)));
    }
    lfp = (((tstamp)seconds) << 32) | ((tstamp)fractions);
    return lfp;
}

/*
*   Function to read chars from header and convert them to NTP short format
*   Input: char header, starting position of time to convert
*   Output: Time in tdist format
*/

tstamp char_to_tdist(char *header, int index)
{

    tdist fp = 0;
    for (int i = 0; i < 4; i++)
    {
        tstamp bla = header[index];
        fp += (header[index] << (24 - 8 * i));
        index++;
    }
    return fp;
}

int main(int argc, char *argv[])
{
    int number_of_servers = argc - 1; //copy servers from console
    char *server[number_of_servers];
    for (int i = 0; i < number_of_servers; i++)
        server[i] = argv[i + 1];

    message *msg;
    double max_delay;
    double min_delay;
    double min_dispersion = 0;
    double min_root_dispersion = 0;
    int client_socket;
    int result;
    char *best_server = NULL;
    double average_offset = 0;
    fd_set readfds;
    int i;

    //boring structs for sockets
    struct addrinfo *results;
    struct addrinfo socket_to_conect_config;
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    memset(&socket_to_conect_config, 0, sizeof(socket_to_conect_config));
    socket_to_conect_config.ai_family = AF_INET;
    socket_to_conect_config.ai_socktype = SOCK_DGRAM;

    if (result == -1)
    {
        perror("getaddrinfo error");
        exit(1);
    }

    double best_server_dispersion = 0;

    for (int j = 0; j < number_of_servers; j++)
    {
        result = getaddrinfo(server[j], UDP, &socket_to_conect_config, &results);
        client_socket = socket(AF_INET, SOCK_DGRAM, 0);
        max_delay = 0;
        min_delay = 0;
        double sum_of_delay = 0;
        double sum_of_offset = 0;
        double sum_of_root_dispersion = 0;
        double dispersion = 0;
        int n = client_socket + 1;

        for (i = 0; i < 8; i++) //for later change to 8
        {
            unsigned char buffer[SIZE];
            memset(buffer, 0, SIZE);
            buffer[0] = 0b00100011;
            msg = calloc(sizeof(message), SIZE);
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);
            struct timeval tv = {1, 0};
            tstamp origin = get_time();
            sendto(client_socket, buffer, SIZE, 0, results->ai_addr, results->ai_addrlen); //send message
            if (select(n, &readfds, NULL, NULL, &tv) == 0)
            {
                printf("Server %s doesn't respond.\n", server[j]);
                i--;
                break;
            }
            //TODO mit connect?
            recvfrom(client_socket, &buffer, SIZE, 0, (struct sockaddr *)&src_addr, &src_addr_len); //receive answer
            tstamp destination = get_time();                                                        //get "destination" clock
            //parse the message
            msg->header = buffer[0];
            msg->stratum = buffer[1];
            msg->poll = buffer[2];
            msg->precision = buffer[3];
            msg->delay = char_to_tdist(buffer, 4);
            msg->dispersion = char_to_tdist(buffer, 8);

            msg->reference_id = (buffer[12] << 24) + (buffer[13] << 16) + (buffer[14] << 8) +
                                buffer[15]; //leaving like that since ref_id was meant to be a char
            msg->reference = char_to_tstamp(buffer, 16);
            msg->receive = char_to_tstamp(buffer, 32);
            msg->transmit = char_to_tstamp(buffer, 40);

            //            Reference Timestamp: Time when the system clock was last set or corrected, in NTP timestamp format.
            //
            //            Origin Timestamp (org): Time at the client when the request departed for the server, in NTP timestamp format.
            //
            //            Receive Timestamp (rec): Time at the server when the request arrived from the client, in NTP timestamp format.
            //
            //            Transmit Timestamp (xmt): Time at the server when the response left the client, in NTP timestamp format.
            //
            //            Destination Timestamp (dst): Time at the client when the reply arrived from the server, in NTP timestamp format.
            //
            //            Note: The Destination Timestamp field is not included as a header field; it is determined upon arrival of the packet and made available in the packet buffer data structure.

            /*printf("\nTime at reference:   %f \n", LFP2D(msg->reference));
            printf("Time at origin:      %f \n", LFP2D(origin));
            printf("Time at receive:     %f \n", LFP2D(msg->receive));
            printf("Time at transmit:    %f \n", LFP2D(msg->transmit));
            printf("Time at destination: %f \n", LFP2D(destination));
            */
            //message parsed*/

            double delay = ((LFP2D(destination) - LFP2D(origin)) - (LFP2D(msg->transmit) - LFP2D(msg->receive))) /
                           2; // - instead of  + ?
            double offset = ((LFP2D(msg->receive) - LFP2D(origin)) + (LFP2D(msg->transmit) - LFP2D(destination))) / 2;
            printf("Delay: %.9f \n", delay);
            printf("Offset: %.9f \n", offset);
            //printf("root_dispersion: %.9f \n", LFP2D(msg->dispersion));

            sum_of_delay += delay;
            sum_of_offset += offset;
            sum_of_root_dispersion += LFP2D(msg->dispersion);

            if (j == 0 && i == 0)
            {
                max_delay = delay;
                min_delay = delay;
            }
            else
            {
                if (delay > max_delay)
                    max_delay = delay;
                if (delay < min_delay)
                    min_delay = delay;
            }
        }

        double root_dispersion = sum_of_root_dispersion / i;
        dispersion = max_delay - min_delay;
        double dispersion_criteria = root_dispersion + dispersion;
        printf("\nnumber of answered queries: %i\n", i);
        printf("dispersion: %.9f \n", dispersion);
        printf("root_dispersion: %.9f \n", root_dispersion);
        printf("max_delay: %.9f \n", max_delay);
        printf("min_delay: %.9f \n", min_delay);
        printf("sum offset: %.9f\n", sum_of_offset);
        printf("sum delay:  %.9f\n", sum_of_delay);
        printf("sum root_dispersion:  %f\n", sum_of_root_dispersion);
        printf("dispersion_criteria: %f\n", dispersion_criteria);
        //choose best server
        if (j == 0)
        {
            best_server_dispersion = dispersion_criteria;
            best_server = server[j];
            average_offset = sum_of_offset / i;
        }
        else
        {
            if (dispersion_criteria < best_server_dispersion)
            {
                best_server_dispersion = dispersion_criteria;
                best_server = server[j];
                average_offset = sum_of_offset / i;
            }
        }
        printf("best_server_dispersion: %.9f\n", best_server_dispersion);

        printf("\n{%s} {%.9f} {%.9f} {%.9f} {%.9f}\n", server[j], sum_of_root_dispersion / i, dispersion, sum_of_delay / i,
               sum_of_offset / i);
        close(client_socket);
    }

    //TODO not tested and might not be right values
    tstamp local = get_time();
    printf("chosen server: %s\n", best_server);
    printf("local time:     %f\n", LFP2D(local));
    printf("average offset: %f\n", average_offset);
    printf("corrected time: %f\n",
           LFP2D(local) + average_offset); //TODO works for negative and is it even right formula?

    freeaddrinfo(results);
}

/*struct timeval unix_time;
gettimeofday(&unix_time, NULL);                             //Get current UNIX time
unsigned int seconds = unix_time.tv_sec + UNIX_TIME_OFFSET; //Add UNIX time offset
unsigned int fractions = unix_time.tv_usec / 1e6 * FRAC;    //Convert miliseconds to fractions
//seconds = htonl(seconds);
//seconds = htonl(fractions);
unsigned int *seconds_pointer = &seconds;
unsigned int *fractions_pointer = &fractions;
printf("AFTER CONVERSION\n");
seconds = htonl(seconds);
fractions = htonl(fractions);
memcpy(buffer + 24, seconds_pointer, 4);
memcpy(buffer + 28, fractions_pointer, 4);
/* for testing
            printf("%x\n", buffer[24]);
            printf("%x\n", buffer[25]);
            printf("%x\n", buffer[26]);
            printf("%x\n", buffer[27]);
            printf("%x\n", buffer[28]);
            printf("%x\n", buffer[29]);
            printf("%x\n", buffer[30]);
            printf("%x\n", buffer[31]);
            //printf("%u seconds\n", seconds);
            //printf("%u fractions\n", fractions);
            */
