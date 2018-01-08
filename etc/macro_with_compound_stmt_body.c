#define DO_SOMETHING(x) { \
    x = x + 2;  \
}

int main(int argc, char** argv) {
    DO_SOMETHING(argc);
    return argc;
}
