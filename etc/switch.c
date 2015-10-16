#include <stdio.h>

int main(int argc, char** argv){
  switch( argc ) {
    case 1: printf("%s\n", "ONE");
      break;
    case 2: printf("%s\n", "TWO");
      break;
    default: 
      printf("%s\n", "THREE");
      printf("%s\n", "OR MORE");
      break;
  }
  return 0;
}
