#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char secret[8];
  char *end = sbrk(36 * PGSIZE);
  end = end + 16 * PGSIZE + 32;
  strcpy(secret, end);
  write(2, secret, 8);
  exit(1);
}
