#include <string.h>
#include <stdio.h>

#define print_string_len(s) \
  do { \
      int l = strlen(s); \
      printf("%d\n", l); \
  } while (0)

int main(int argc, char** argv) {
    print_string_len(argv[0]);
    return 0;
}
