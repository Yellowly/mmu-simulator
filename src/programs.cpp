#include "programs.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>

int MapperProgram::main(int argc, char *argv[]) {
  char buf[100];
  int fd = open(argv[1], O_RDONLY);
  read(fd, buf, sizeof(buf));
  printf("Received: %s\n", buf);
  close(fd);

  vaddr<int> addrs = my_mmap(0, 1024);

  *addrs = 12345;

  // Try changing this to (0, 256) and see if it segfaults!
  my_munmap(256, 256);

  int temp = *addrs;
  printf("Addrs %d\n", temp);

  // This should result in a segfault
  temp = *(addrs + 270);
  printf("Addrs %d\n", temp);

  return 0;
}
