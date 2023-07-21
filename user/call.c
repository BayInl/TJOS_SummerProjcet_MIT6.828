#include "../kernel/param.h"
#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

void main(void) {
  printf("%d %d\n", f(8)+1, 13);
  printf("t=%d\n");
  printf("x=%d y=%d\n", 3);
  printf("%d %d\n", f(8)+1, 13);
  printf("t=%d\n");
  printf("x=%d y=%d\n", 3);
  exit(0);
}
