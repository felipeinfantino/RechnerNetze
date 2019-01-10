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
#include <endian.h>

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
//

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

unsigned int little_to_big(unsigned int little)
{
    return (((little >> 24) & 0x000000ff) | ((little >> 8) & 0x0000ff00) | ((little << 8) & 0x00ff0000) | ((little << 24) & 0xff000000));
}
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
    seconds = htonl(seconds);
    fractions = htonl(fractions);
    current_time = (((tstamp)seconds) << 32) | ((tstamp)fractions); //TODO: need to play with bits here, its in wrong order because of changing endianness
    //printf("%u\n", ntohl(seconds));
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
    /*  if you want to see the output
    printf("%u \n", seconds);
    printf("%u \n", fractions);
    printf("%f \n", LFP2D(lfp));
    */
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
    double root_dispersion = 0;
    int client_socket;
    int result;

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
    for (int j = 0; j < number_of_servers; j++)
    {
        result = getaddrinfo(server[j], UDP, &socket_to_conect_config, &results);
        client_socket = socket(AF_INET, SOCK_DGRAM, 0);
        max_delay = 0;
        min_delay = 0;
        int sum_of_delay = 0;
        int sum_of_offset = 0;
        for (int i = 0; i < 1; i++)
        {
            unsigned char response[SIZE];
            msg = calloc(sizeof(message), SIZE);
            msg->header = 0b00100011; //SET First byte according to protocol
            //TODO: write a function/ find a way to flip bits in
            msg->origin = (get_time()); //get current origin clock and pack it into the message  <- also a problem, needs bit flipping, since the server doesnt answer with it
            printf("%f\n", LFP2D(msg->origin));
            sendto(client_socket, msg, SIZE, 0, results->ai_addr, results->ai_addrlen);                   //send message
            recvfrom(client_socket, &response, SIZE + 1, 0, (struct sockaddr *)&src_addr, &src_addr_len); //receive answer
            tstamp destination = get_time();                                                              //get "destination" clock
            //parse the message
            msg->header = response[0];
            msg->stratum = response[1];
            msg->poll = response[2];
            msg->precision = response[3];
            msg->delay = char_to_tdist(response, 4);
            msg->dispersion = char_to_tdist(response, 8);
            msg->reference_id = (response[12] << 24) + (response[13] << 16) + (response[14] << 8) + response[15]; //leaving like that since ref_id was meant to be a char
            msg->reference = char_to_tstamp(response, 16);
            msg->origin = char_to_tstamp(response, 24);
            msg->receive = char_to_tstamp(response, 32);
            msg->transmit = char_to_tstamp(response, 40);
            //message parsed
            printf("%f\n", LFP2D(msg->origin));
            printf("%f\n", LFP2D(msg->receive));
            //calculate delay, offset (delay=delta, offset=theta)
            double delay = ((LFP2D(destination) - LFP2D(msg->origin)) + ((LFP2D(msg->transmit) - LFP2D(msg->receive))) / 2);
            double offset = (LFP2D(msg->receive) - LFP2D(msg->origin)) + (LFP2D(msg->transmit) - LFP2D(destination)) / 2;
            sum_of_delay += delay;
            sum_of_offset += offset;
            if (i == 0) //update max_delay
                max_delay = delay;
            else if (max_delay < delay)
                max_delay = delay;

            if (i == 0) //update min_delay
                min_delay = offset;
            else if (min_delay > offset)
                min_delay = offset;
        }

        /*TODO: 
        1) if u solve the problem above pretty much everything else is done
        */

        if (j == 0)
            root_dispersion = msg->dispersion;
        else if (root_dispersion > msg->dispersion)
            root_dispersion = msg->dispersion;

        tstamp current_dispersion = max_delay - min_delay;
        if (j == 0)
            min_dispersion = current_dispersion;
        else if (min_dispersion > current_dispersion)
            min_dispersion = current_dispersion;

        //print_information();
        close(client_socket);
    }

    freeaddrinfo(results);
}
