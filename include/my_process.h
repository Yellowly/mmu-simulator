#ifndef __MY_PROCESS_H
#define __MY_PROCESS_H
#include "my_mmu.h"
#include <list>
#include <pthread.h>
#include <type_traits>
// #define PAGE_SIZE 256

// #define MYMAP_FAILED vaddr<void>(NULL, 0, -1);

/**
 * @brief A virtual memory area
 */
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
  virtual ~Program() = default;

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
  /**
   * @brief Initialize a process which runs the associated program
   *
   * ```
   * // BasicTestProgram is a class inheriting from Program
   * Process a = Process(progT<BasicTestProgram>{}, &mmu);
   * ```
   */
  template <typename T> Process(progT<T>, MMU *mmu) {
    static_assert(std::is_base_of_v<Program, T>,
                  "Process<T>: T must derive from Program");
    init_pgd(mmu);

    prog = new T();
    prog->set_proc(this);
  }
  ~Process();

  // This should probably be private
  void init_pgd(MMU *mmu);

  MMU *get_mmu() { return pinfo.mmu; }
  unsigned int get_pgd_addr() { return pinfo.pgd_addr; }

  /**
   * @brief Casts a virtual address into the vaddr abstraction
   */
  template <typename T>
  inline vaddr<T> get_vaddr(unsigned int virtual_address) {
    return vaddr<T>(&pinfo, virtual_address);
  }

  /**
   * @brief Convert a virtual address for this process into a physical memory
   * address
   * @param virtual_address The virtual address being turned into a physical
   * address
   * @return Physical address (ie: byte offset into the mmu's `physical_memory`)
   * or -1 if the virtual memory mapping doesn't exist
   */
  unsigned int virt_to_phys(unsigned int virtual_address);

  /**
   * @brief Map a virtual memory region to physical memory.
   * @param Starting virtual address to map
   * @param length Length of the virtual address region being mapped
   * @return Starting address of the mapped memory region (aligned to
   * `page_size`)
   */
  unsigned int my_mmap(unsigned int start, unsigned int length);

  /**
   * @brief Unmap a virtual memory region and free associated physical pages.
   * @param Starting virtual address to unmap
   * @param length Length of the virtual address region being unmapped
   * @return 0(?)
   */
  int my_munmap(unsigned int start, unsigned int length);

  /**
   * @brief Run the program associated with this process using the provided
   * arguments
   */
  int run(int argc, char *argv[]);

  /**
   * @brief Reap this process
   */
  int wait(int *status);

  /**
   * @brief Free all the memory pages used by this process
   */
  void cleanup();
};

#endif
