#include <stdio.h>

int print_i_times(int i) {
  while (i > 0) {
    puts("Hello world!");
    i--;
  }
  return 0;
}

int main(int argc, char** argv) {
  print_i_times(10);
  return 0;
}
