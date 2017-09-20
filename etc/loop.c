#include <stdio.h>

int main(int argc, char *argv[])
{
  int i=0,
      j=10;

  for(i=0,j=10; i<10 && j>0; i++,j--)
    {
      printf("hello %d %d\n", i, j);
    }

  return 0;
}
