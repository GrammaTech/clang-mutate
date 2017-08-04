#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    int a = open(argv[1], O_RDONLY, S_IREAD);
}
