#include "alloca_macro.h"

int main(int argc, char** argv) {
    char *a = (char*) ALLOCA(4*sizeof(char));
}
