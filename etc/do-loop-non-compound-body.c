#include <stdio.h>

int main(int argc, char** argv) {
    int i = 0;
    do
        printf("%s\n", argv[i++]);
    while (i < argc);

    return 0;
}
