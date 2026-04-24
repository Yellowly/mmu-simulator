#ifndef __MY_PROGRAMS_H
#define __MY_PROGRAMS_H

#include "my_process.h"
#include <unistd.h>

class BasicTestProgram : public Program {
public:
  int main(int argc, char *argv[]);
};

class MapperProgram : public Program {
public:
  int main(int argc, char *argv[]);
};

#endif
