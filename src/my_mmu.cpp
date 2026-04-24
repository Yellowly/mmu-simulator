#include "my_mmu.h"
#include <cstdio>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <sys/mman.h>

/**
 * @brief Initialize the MMU
 *
 * - Use mmap to initialize the physical memory space
 * - Set `page_size`, `num_pages` using `mem_size` and `page_size` arguments
 *   (Note mem_size is the number of bytes we want in physical memory)
 *   (Also note, no matter the mem_size, the actual amount of memory that gets
 *   allocated should be a multiple of page size. num_pages should contain
 *   enough pages to fit all the pages)
 * - pthread_mutex_init the `free_pages_lock`
 * - Initialize the `free_pages` list to contain the pfn of all pages
 *   (If num_pages = 256, free_pages should contain 0 to 255)
 *
 * Note that in testing, we will only be testing page_size = 256. Given the
 * layout of our virtual addresses, the MMU will not work if page_size < 256,
 * and there will be wasted memory if page_size > 256
 */
MMU::MMU(unsigned int mem_size, unsigned int page_size) {
  physical_memory =
      mmap(nullptr, (mem_size / page_size) * page_size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  this->page_size = page_size;
  pthread_mutex_init(&free_pages_lock, NULL);

  num_pages = (mem_size - 1) / page_size + 1;
  for (unsigned int i = 0; i < num_pages; i++) {
    free_pages.push_back(i);
  }
}

MMU::~MMU() {
  munmap(physical_memory, num_pages * page_size);
  pthread_mutex_destroy(&free_pages_lock);
}

/**
 * @brief Get the page frame number (pfn) of a free page
 *
 * Make sure to use lock the free pages before popping
 * After popping the page, use `memset()` to reset the entire memory page to 0s
 *
 * @return A page frame number (pfn)
 */
unsigned int MMU::get_free_page() {
  pthread_mutex_lock(&free_pages_lock);
  unsigned int pfn = free_pages.front();
  free_pages.pop_front();
  pthread_mutex_unlock(&free_pages_lock);
  memset((char *)physical_memory + get_page(pfn), 0, page_size);
  return pfn;
}

/**
 * @brief Get the physical address of a page given its pfn
 *
 * Remember that physical addresses are simply unsigned ints
 *
 * @return Physical address of the given page
 * (ie: the byte index of the page in physical memory)
 *
 * To convert this byte index into a pointer to the page, use:
 * `(char*)physical_memory + get_page(pfn)`
 */
unsigned int MMU::get_page(unsigned int pfn) { return page_size * pfn; }

/**
 * @brief Free a page
 *
 * Should re-add the given pfn to the `free_pages` list
 *
 * Remember to lock free pages before pushing!
 */
void MMU::add_free_page(unsigned int pfn) {
  pthread_mutex_lock(&free_pages_lock);
  free_pages.push_back(pfn);
  pthread_mutex_unlock(&free_pages_lock);
}

/**
 * @brief Get an entry given the pfn of a page table, and an offset
 * (Page tables refers to PGDs, PMDs, and PTEs - not physical pages)
 *
 * The 24 most significant bits of a page_entry refer to the PFN of another page
 * table.
 * The 8 least significant bits of a page_entry are used as flags for that
 * page
 * For this project, the only bit you have to worry about is the 1 least
 * significant bit, which is used to flag the page as 'in use'
 *
 * Additionally, note that offset is a page_entry offset, not a byte offset, so
 * make sure to cast your pointers accordingly!
 */
page_entry MMU::get_page_entry(int pfn, int offset) {
  return *(((page_entry *)((char *)physical_memory + get_page(pfn))) + offset);
}

/**
 * @brief Set an entry given the pfn of a page table, and an offset
 */
void MMU::set_page_entry(int pfn, unsigned int offset, page_entry val) {
  *(((page_entry *)((char *)physical_memory + get_page(pfn))) + offset) = val;
}

/**
 * @brief Get the page middle directory frame number (pmd pfn)
 *
 * The 20th-25th least significant bits of a virtual address are used to offset
 * into the PGD (You can do `(vaddr >> PGD_OFFSET) & 0x3F` to extract these
 * bits)
 *
 * Hint: Use get_page_entry
 * Hint 2: Recall the format of the `page_entry` returned by get_page_entry!
 *
 * @return The pfn of the pmd if the page entry is marked as in use,
 * otherwise -1
 */
int MMU::get_pmd(int pgd_pfn, unsigned int vaddr) {
  unsigned int pmd = get_page_entry(pgd_pfn, (vaddr >> PGD_OFFSET) & 0x3F);
  return (pmd & 0x1) == 1 ? pmd >> 8 : -1;
}

/**
 * @brief Get the page table entries frame number (pte pfn)
 *
 * The 14th-19th least significant bits of a virtual address are used to offset
 * into the PMD
 *
 * @return The pfn of the pte if the page entry is marked as in use,
 * otherwise -1
 */
int MMU::get_pte(int pmd_pfn, unsigned int vaddr) {
  unsigned int pte = get_page_entry(pmd_pfn, (vaddr >> PMD_OFFSET) & 0x3F);
  return (pte & 0x1) == 1 ? pte >> 8 : -1;
}

/**
 * @brief Get the physical page frame number (phys pfn)
 *
 * The 8th-13th least significant bits of a virtual address are used to offset
 * into the PTE
 *
 * @return The pfn of the physical page if the page entry is marked as in use,
 * otherwise -1
 */
int MMU::get_phys(int pte_pfn, unsigned int vaddr) {
  unsigned int phys = get_page_entry(pte_pfn, (vaddr >> PTE_OFFSET) & 0x3F);
  return (phys & 0x1) == 1 ? phys >> 8 : -1;
}

/**
 * @brief Perform a page walk and get a pointer to the location in physical
 * memory corresponding to the given virtual address
 *
 *  Throw a runtime error if any of the page frame numbers returned are -1:
 * `throw std::runtime_error("Segfault");` (You can change the error message to
 * be more descriptive if you want)
 *
 * The 8 least significant bits of the virtual address are used to offset into a
 * physical page
 *
 * @return A pointer to a physical memory location corresponding to vaddr
 */
void *MMU::page_walk(unsigned int pgd_addr, unsigned int vaddr) {
  int pfn;
  if ((pfn = get_pmd(pgd_addr / page_size, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting pmd!");
  }
  if ((pfn = get_pte(pfn, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting pte!");
  }
  if ((pfn = get_phys(pfn, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting physical page!");
  }
  return (((char *)physical_memory + get_page(pfn)) + (vaddr & 0xFF));
}
