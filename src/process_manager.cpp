#include "process_manager.h"
#include <algorithm>
#include <iostream>
#include <pthread.h>

// Functions for running the program associated with this process
void *Process::start_prog(void *args) {
  prog_args p_args = *(prog_args *)args;
  try {
    *p_args.result_loc = p_args.prog->main(p_args.argc, p_args.argv);
  } catch (const std::exception &e) {
    std::cout << "Error in " << p_args.argv[0] << ": " << e.what() << std::endl;
    *p_args.result_loc = 139; // Error code for segfault
  }
  return (void *)p_args.result_loc;
}

int Process::run(int argc, char *argv[]) {
  args = {prog, &result, argc, argv};
  return pthread_create(&thread_id, NULL, start_prog, (void *)&args);
}

int Process::wait(int *status) {
  int res = pthread_join(thread_id, NULL);
  if (status != NULL)
    *status = result;
  return res;
}

/** Docs
 *
 *
 */
// template <typename T> Process::Process(progT<T>, MMU *mmu) {
//   static_assert(std::is_base_of_v<Program, T>,
//                 "Process<T>: T must derive from Program");
//   this->mmu = mmu;
//
//  Get empty page for this process's PGD
//  pgd_addr = mmu->get_page(mmu->get_free_page());
//
//  prog = new T(this);
//}

Process::~Process() { delete prog; }

unsigned int Process::virt_to_phys(unsigned int virtual_address) {
  MMU *mmu = get_mmu();
  unsigned int page_size = mmu->get_page_size();
  int pfn;
  if ((pfn = mmu->get_pmd(get_pgd_addr() / page_size, virtual_address)) == -1) {
    return -1;
  }
  if ((pfn = mmu->get_pte(pfn, virtual_address)) == -1) {
    return -1;
  }
  if ((pfn = mmu->get_phys(pfn, virtual_address)) == -1) {
    return -1;
  }
  return pfn * page_size + (virtual_address & 0xFF);
}

unsigned int Process::my_mmap(unsigned int start, unsigned int length) {
  for (vm_area a : used_vm_areas) {
    if (start < a.start + a.length && a.start < start + length) {
      return -1;
    }
  }
  MMU *mmu = get_mmu();

  unsigned int page_size = mmu->get_page_size();
  start = (start / page_size) * page_size;

  for (unsigned int i = 0; i < (length - 1) / page_size + 1; i++) {
    unsigned int page_start = start + page_size * i;
    int pfn;
    if ((pfn = mmu->get_pmd(pinfo.pgd_addr / page_size, page_start)) == -1) {
      pfn = mmu->get_free_page();
      mmu->set_page_entry(pinfo.pgd_addr / page_size, (page_start >> 20) & 0x3F,
                          (pfn << 8) | 1);
    }
    unsigned int pmd_pfn = pfn;

    if ((pfn = mmu->get_pte(pfn, page_start)) == -1) {
      pfn = mmu->get_free_page();
      mmu->set_page_entry(pmd_pfn, (page_start >> 14) & 0x3F, (pfn << 8) | 1);
    }
    unsigned int pte_pfn = pfn;

    if ((pfn = mmu->get_phys(pte_pfn, page_start)) == -1) {
      pfn = mmu->get_free_page();
      mmu->set_page_entry(pte_pfn, (page_start >> 8) & 0x3F, (pfn << 8) | 1);
    }
  }

  used_vm_areas.push_back({start, length});
  return start;
}

int Process::my_munmap(unsigned int start, unsigned int length) {
  unsigned int page_size = get_mmu()->get_page_size();

  for (unsigned int i = 0; i < (length - 1) / page_size + 1; i++) {
    unsigned int page_start = start + page_size * i;

    int pfn;
    if ((pfn = get_mmu()->get_pmd(get_pgd_addr() / page_size, page_start)) ==
        -1) {
      continue;
    }
    if ((pfn = get_mmu()->get_pte(pfn, page_start)) == -1) {
      continue;
    }
    unsigned int pte_pfn = pfn;
    if ((pfn = get_mmu()->get_phys(pfn, page_start)) == -1) {
      continue;
    }
    get_mmu()->set_page_entry(pte_pfn, (page_start >> 8) & 0x3F, 0);
    get_mmu()->add_free_page(pfn);
  }

  for (auto it = used_vm_areas.begin(); it != used_vm_areas.end();) {
    vm_area &a = *it;
    unsigned int a_end = a.start + a.length;
    unsigned int new_end = start + length;

    // Check if the range overlaps with the current area
    if (start < a_end && a.start < new_end) {

      // Case: Range is entirely within the area (Split needed)
      if (start > a.start && new_end < a_end) {
        vm_area tail;
        tail.start = new_end;
        tail.length = a_end - new_end;

        a.length = start - a.start;

        // Insert the tail after the current element
        it = used_vm_areas.insert(std::next(it), tail);
        // Resulting 'it' points to tail, so we move past it
        ++it;
        continue;
      }

      // Case: Overlap at the start, middle, or end (Truncation)
      if (start <= a.start) {
        // Cut from the front
        a.start = std::max(a.start, new_end);
        a.length = (a_end > a.start) ? (a_end - a.start) : 0;
      } else {
        // Cut from the back
        a.length = start - a.start;
      }
    }
    if (a.length == 0)
      it = used_vm_areas.erase(it);
    else {
      *it = a;
      ++it;
    }
  }
  return 0;
}

Program::Program(Process *p) { proc = p; }

unsigned int Program::virt_to_phys(unsigned int virtual_address) {
  return proc->virt_to_phys(virtual_address);
}
vaddr<void> Program::my_mmap(unsigned int start, unsigned int length) {
  return proc->get_vaddr<void>(proc->my_mmap(start, length));
}
vaddr<void> Program::my_munmap(unsigned int start, unsigned int length) {
  return proc->get_vaddr<void>(proc->my_munmap(start, length));
}
