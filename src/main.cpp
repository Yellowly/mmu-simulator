#include "my_mmu.h"
#include "my_process.h"
#include "programs.h"
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  MMU mmu = MMU(65536, 256);
  // MapperProgram a = MapperProgram(mmu);
  std::cout << "mmu start free size: " << mmu.get_free_pages_size() << "\n";

  {
    Process a = Process(progT<BasicTestProgram>{}, &mmu);
    Process b = Process(progT<BasicTestProgram>{}, &mmu);

    char *pipe1 = "pipe1";
    char *pipe2 = "pipe2";
    // mkfifo(pipe1, 0666);
    // mkfifo(pipe2, 0666);

    char *arg1s[] = {"program 1", pipe1};
    char *arg2s[] = {"program 2", pipe2};
    a.run(2, arg1s);
    b.run(2, arg2s);

    // int fd1 = open(pipe1, O_WRONLY);
    // int fd2 = open(pipe2, O_WRONLY);

    // write(fd1, "Hello,", 7);
    // write(fd2, "World!", 7);
    // close(fd1);
    // close(fd2);

    // std::cout << "waiting...\n";
    a.wait(NULL);
    b.wait(NULL);
  }

  std::cout << "mmu end free size: " << mmu.get_free_pages_size() << "\n";
  // unlink(pipe1);
  // unlink(pipe2);
}
