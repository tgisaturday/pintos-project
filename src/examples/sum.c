#include <stdio.h>
#include <syscall.h>
#include <stdlib.h>
int
main (int argc, char*argv[])
{
    printf ("%d %d\n", pibonacci(atoi(argv[1])),sum_of_four_integers(atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),atoi(argv[4])));

  return 0;
}
