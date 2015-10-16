#include <stdio.h>

int main(int argc, char** argv) {
  int out = (argc == 1) ? 
            1 : 
            2;

  printf("%d\n", out); 
  return 0;
}
