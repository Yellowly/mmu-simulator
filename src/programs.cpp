#include "programs.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>

int BasicTestProgram::main(int argc, char *argv[]) {
  // mmap some pages
  vaddr<int> addrs = my_mmap(0, 1024);

  *addrs = 12345;

  // Should get 12345
  int temp = *addrs;
  printf("Addr test 1 %d\n", temp);

  // Try changing this to (0, 256) and see if the next line segfaults!
  my_munmap(256, 256);

  addrs[1] = 67890;
  temp = *(addrs + 1);
  printf("Addr test 2 %d\n", temp);

  // This shouldn't result in a segfault
  temp = *(addrs + 200);
  printf("Addr test 3 %d\n", temp);

  // This should result in a segfault (between 256 and 512)
  temp = *(addrs + 80);
  printf("Addr test 4 %d\n", temp);

  // This should also result in a segfault (>1024)
  temp = *(addrs + 300);
  printf("Addr test 5 %d\n", temp);

  return 0;
}

int MapperProgram::main(int argc, char *argv[]) {
  // TODO: Interactive memory mapping program
  char buf[100];
  int fd = open(argv[1], O_RDONLY);
  read(fd, buf, sizeof(buf));
  printf("Received: %s\n", buf);
  close(fd);

  return 0;
}
