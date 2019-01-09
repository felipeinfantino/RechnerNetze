#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int hash(const char *str, unsigned int length)
{
    unsigned int hash = 5381;
    unsigned int i = 0;

    for (i = 0; i < length; ++str, ++i)
    {
        hash = ((hash << 5) + hash) + (*str);
    }

    return hash % 65536;
}

int main(int argc, char const *argv[])
{

    while (1)
    {
        char word[1000];
        scanf("%s", word);
        if (word != NULL)
        {
            int length = strlen(word);
            if (length > 0)
                printf("%d\n", hash(word, strlen(word)));
        }
    }
    return 0;
}
