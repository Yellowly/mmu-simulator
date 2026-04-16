#ifndef __MY_PROGRAMS_H
#define __MY_PROGRAMS_H

#include "process_manager.h"
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
