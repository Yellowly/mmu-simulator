#include "my_process.h"
#include <algorithm>
#include <iostream>
#include <pthread.h>

// Functions for running the program associated with this process
// Note that for this project, processes and programs are abstracted as simple
// threads as opposed to actual new processes
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

// Run the program associated with this process
int Process::run(int argc, char *argv[]) {
  args = {prog, &result, argc, argv};
  return pthread_create(&thread_id, NULL, start_prog, (void *)&args);
}

// Wait for the program to finish (ie: reap the process)
int Process::wait(int *status) {
  int res = pthread_join(thread_id, NULL);
  if (status != NULL)
    *status = result;
  return res;
}

// After reaping a process, its destructor should be called to free all the
// memory pages allocated to it
Process::~Process() {
  cleanup();
  delete prog;
}

/**
 * @brief Initialize the page global directory (pgd) of this process
 *
 * Set pinfo.mmu to the mmu argument
 * Set pinfo.pgd_addr to the *physical address* of a new free page that you
 * request from the mmu
 */
void Process::init_pgd(MMU *mmu) {
  pinfo.mmu = mmu;

  // Get physical address of empty page for this process's PGD
  pinfo.pgd_addr = mmu->get_page(mmu->get_free_page());
}

/**
 * @brief Convert a virtual address for this process into a physical memory
 * address
 *
 * This should be similar to `page_walk`, except you get the PGD address from
 * `pinfo.pgd_addr` and you return -1 instead of raising an error
 *
 * @return Physical address (ie: byte offset into the mmu's `physical_memory`)
 * -1 if the virtual memory mapping doesn't exist
 */
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

/**
 * @brief Map a virtual memory region to physical memory.
 *
 * Step 1 - Check for overlap:
 * - Iterate through used_vm_areas
 * - Check if [start, start+length) overlaps with any area
 * - Return -1 if there is an overlap
 *
 * Step 2 - Align addresses:
 * - Align start to the page size
 * - Create a loop that runs for every page between the start, start+length
 *   addresses,
 *
 * Step 3 - Page walk:
 * - For every requested virtual page, get the start address of virtual page
 * - Perform a page walk using `get_mmu()->get_pmd(...)` (and the other
 *   functions you wrote)
 * - If any of the pagewalk functions returns -1 (ie: the memory page is marked
 *   as not in use), then:
 *   - Request a new free page (`get_mmu()->get_free_page()`)
 *   - Set the page entry to your new page (recall that the most significant 24
 *     bits of a page entry correspond to the pfn of the new page, and the least
 *     significnat bit is the `in-use` flag
 *   - Continue running the rest of the page walk as usual after initializing
 *     the new page
 *
 * Step 4 - Add used virtual memory area:
 * - Append {start, length} to the `used_vm_areas`
 *
 * @return Starting address of the mapped memory region (aligned to `page_size`)
 */
unsigned int Process::my_mmap(unsigned int start, unsigned int length) {
  // Search used virtual memory areas and check if there is overlap
  for (vm_area a : used_vm_areas) {
    if (start < a.start + a.length && a.start < start + length) {
      return -1;
    }
  }

  // Get starting address for mmap allocation
  MMU *mmu = get_mmu();

  unsigned int page_size = mmu->get_page_size();
  start = (start / page_size) * page_size;

  // For every page that we request
  for (unsigned int i = 0; i < (length - 1) / page_size + 1; i++) {
    // Get start address of the new page we request
    unsigned int page_start = start + page_size * i;
    int pfn;

    // Perform page walk and request pages as necessary
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

  // Add the used virtual memory address region
  used_vm_areas.push_back({start, length});
  return start;
}

/**
 * @brief Unmap a virtual memory region and free associated physical pages.
 *
 * Step 1 - Define loop:
 * - Iterate through each page containing addresses in the range
 *   [start, start+length)
 *
 * Step 2 - Page walk:
 * - Perform a page walk using `get_mmu()->get_pmd(...)` (and the other pagewalk
 *   functions you defined)
 * - If the page is not in use, then simply continue to the next iteration of
 *   the for loop
 * - Free the physical memory page associated with the virtual address:
 *   - Add the pfn of the physical memory page to the mmu's free page list
 *   - Mark the corresponding PTE entry containing the physical page's pfn as
 *     not in use
 * - Note that for simplicity, you DO NOT need to recursively free higher-level
 *   page tables. You only need to free the physical pages for this project
 *
 * Step 3 - Update virtual memory areas:
 * - Iterate through all used virtual memory areas (`used_vm_areas`)
 * - If the munmap region does not intersect with a vm_area, continue the loop
 * - If [start, start+length) is entirely contained within one vm area, you will
 *   need to split the area
 * - Otherwise, simply truncate the existing overlapping area to remove the
 *   [start, start+length) address range from the vm area
 * - If the newly modified vm area has 0 length, remove it from the list
 *   entirely
 *
 * @return 0
 */
int Process::my_munmap(unsigned int start, unsigned int length) {
  unsigned int page_size = get_mmu()->get_page_size();

  // For all the virtual addresses we want to free
  for (unsigned int i = 0; i < (length - 1) / page_size + 1; i++) {
    unsigned int page_start = start + page_size * i;

    // Perform page walk
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
    // Mark the associated physical page as not in use, and re-add it to the
    // mmu's free list
    get_mmu()->set_page_entry(pte_pfn, (page_start >> 8) & 0x3F, 0);
    get_mmu()->add_free_page(pfn);
  }

  // Remove the munmap region from used virtual memory areas
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

/**
 * @brief Free all the memory pages used by this process
 *
 * This will involve iterating through every entry of every in-use page table of
 * this process, and freeing the associated page using
 * `get_mmu()->add_free_page(...)`
 *
 * This will involve 3 nested for loops (one for for each page tabel level)
 *
 * You should not need to mark the pages as no longer in use, since pages will
 * get filled with 0s when another process requests them (as per your
 * `MMU::get_free_page()` implementation).
 */
void Process::cleanup() {
  unsigned int page_size = get_mmu()->get_page_size();
  unsigned int num_entries = page_size / 4;
  MMU *mmu = get_mmu();
  int pgd_pfn = pinfo.pgd_addr / page_size;
  // For every entry in the PGD
  for (unsigned int i = 0; i < num_entries; i++) {
    int pmd_pfn;
    if ((pmd_pfn = mmu->get_pmd(pgd_pfn, i << 20)) != -1) {
      // For every entry in a PMD
      for (unsigned int j = 0; j < num_entries; j++) {
        int pte_pfn;
        if ((pte_pfn = mmu->get_pte(pmd_pfn, j << 14)) != -1) {
          // For every entry in a PTE
          for (unsigned int k = 0; k < num_entries; k++) {
            int phys_pfn;
            if ((phys_pfn = mmu->get_phys(pte_pfn, k << 8)) != -1) {
              mmu->add_free_page(phys_pfn);
            }
          }
          mmu->add_free_page(pte_pfn);
        }
      }
      mmu->add_free_page(pmd_pfn);
    }
  }
  mmu->add_free_page(pgd_pfn);
}

//
// #########################################
// Define abstract class for a program
// #########################################
//
// (You do not neet to implement anything here)

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
