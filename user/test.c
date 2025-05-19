#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd = open("test.txt", O_CREATE | O_WRONLY);
  write(fd, "hello", 5);
  close(fd);
  fd = open("test.txt", O_RDONLY);
  char buf1[100], buf2[100];
  read(fd, buf1, 6);
  close(fd);
  fd = open("test.txt", O_WRONLY);
  write(fd, "world", 5);
  close(fd);
  fd = open("test.txt", O_RDONLY);
  read(fd, buf2, 12);
  printf("%s\n%s\n%s\n", buf1, buf2, buf2+6);
}
