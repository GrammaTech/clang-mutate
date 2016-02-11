#include <stdio.h>

#define SIDELENGTH 1001

void bogus()
{

    int diag = 1;
    diag += diag*diag;
    int sum = 1;

    while (diag < SIDELENGTH * SIDELENGTH)
    {

        int i;

        for (i = 0; i < 4; ++i)
        {

            sum += diag;

        }

        diag += 2;


    }

    printf("%d\n", sum);
}
int main(int argc, char **argv){}

