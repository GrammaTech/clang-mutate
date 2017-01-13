#include <stdio.h>

/* To shorten example, not using argp */
int main ()
{
  enum compass_direction
  {
    north,
    east,
    south,
    west
  };

  enum compass_direction my_direction;
  my_direction = west;

  return 0;
}
