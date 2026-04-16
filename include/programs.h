#ifndef __MY_PROGRAMS_H
#define __MY_PROGRAMS_H

#include "process_manager.h"
#include <unistd.h>
class MapperProgram : public Program {

public:
  // using Program::Program;
  //  MapperProgram(Process *p);
  int main(int argc, char *argv[]);
};

#endif
