#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    char *blabla = malloc(sizeof(2));
    blabla = "333";
    strcpy(blabla, "fefe");
}
