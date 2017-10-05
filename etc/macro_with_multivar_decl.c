#define MACRO_WITH_MULTIVAR_DECL \
    do { \
        int a, b; \
    } while (0);

int main() {
    MACRO_WITH_MULTIVAR_DECL;
}
