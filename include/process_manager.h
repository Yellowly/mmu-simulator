#ifndef __MY_PROCESS_MANAGER_H
#define __MY_PROCESS_MANAGER_H
#include "my_mmu.h"
#include <list>
#include <pthread.h>
#include <type_traits>
// #define PAGE_SIZE 256

#define MYMAP_FAILED vaddr<void>(NULL, 0, -1);

struct vm_area {
  unsigned int start;
  unsigned int length;
};

class Process;

class Program {
private:
  Process *proc;

public:
  Program() { proc = NULL; }
  Program(Process *p);
  // virtual ~Program() = default;

  void set_proc(Process *p) { proc = p; }
  unsigned int virt_to_phys(unsigned int virtual_address);
  vaddr<void> my_mmap(unsigned int start, unsigned int length);
  vaddr<void> my_munmap(unsigned int start, unsigned int length);

  virtual int main(int argc, char *argv[]) = 0;
};

struct prog_args {
  Program *prog;
  int *result_loc;
  int argc;
  char **argv;
};

template <typename T> struct progT {};

class Process {
private:
  proc_info pinfo;
  std::list<vm_area> used_vm_areas;
  Program *prog;
  pthread_t thread_id;
  prog_args args;
  int result;

  static void *start_prog(void *args);

public:
  template <typename T> Process(progT<T>, MMU *mmu) {
    static_assert(std::is_base_of_v<Program, T>,
                  "Process<T>: T must derive from Program");
    pinfo.mmu = mmu;

    // Get empty page for this process's PGD
    pinfo.pgd_addr = mmu->get_page(mmu->get_free_page());

    prog = new T();
    prog->set_proc(this);
  }
  ~Process();

  MMU *get_mmu() { return pinfo.mmu; }
  unsigned int get_pgd_addr() { return pinfo.pgd_addr; }
  template <typename T>
  inline vaddr<T> get_vaddr(unsigned int virtual_address) {
    return vaddr<T>(&pinfo, virtual_address);
  }

  unsigned int virt_to_phys(unsigned int virtual_address);
  unsigned int my_mmap(unsigned int start, unsigned int length);
  int my_munmap(unsigned int start, unsigned int length);

  int run(int argc, char *argv[]);

  int wait(int *status);
};

#endif
