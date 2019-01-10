#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main()
{
    int time = 0b01011100001101101001011101101000;

    int reversed = ntohs(time);
    printf("%d\n", time);
    printf("%d", reversed);
}
