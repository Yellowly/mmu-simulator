#ifndef __MY_MMU_H
#define __MY_MMU_H
#include <cstring>
#include <list>
#include <pthread.h>
// #define PAGE_SIZE 256

// template <unsigned int page_size = 256, unsigned int num_pages = 65536>

typedef unsigned int page_entry;
#define PGD_OFFSET 20
#define PMD_OFFSET 14
#define PTE_OFFSET 8

class MMU {

private:
  void *physical_memory;
  std::list<unsigned int> free_pages;
  unsigned int page_size;
  unsigned int num_pages;
  pthread_mutex_t free_pages_lock;

public:
  /**
   * @brief Initialize the MMU
   *
   * @param mem_size Physical memory size in bytes
   * @param page_size Size of each memory page in bytes. `page_size` >= 256
   */
  MMU(unsigned int mem_size, unsigned int page_size);
  ~MMU();

  /**
   * @brief Get the size of each memory page in this MMU
   *
   * @return Number of bytes of each memory page
   */
  unsigned int get_page_size() { return page_size; }

  /**
   * @brief Get the length of the `free_pages` list of this MMU
   *
   * @return Number of free pages in this MMU
   */
  unsigned int get_free_pages_size() { return free_pages.size(); }

  /**
   * @brief Get the page frame number (pfn) of a free page
   *
   * @return A page frame number (pfn)
   */
  unsigned int get_free_page();

  /**
   * @brief Get the physical address of a page given its pfn
   * @param pfn The page frame number
   * @return Physical address of the given page
   * (ie: the byte index of the page in physical memory)
   */
  unsigned int get_page(unsigned int pfn);

  /**
   * @brief Re-add a page to the free pages list
   * @param pfn The page frame number of the page to free
   */
  void add_free_page(unsigned int pfn);

  /**
   * @brief Get the page middle directory frame number (pmd pfn)
   * @param pgd_pfn The page frame number of the page global directory to offset
   * into
   * @param vaddr The virtual address, where the 20th-25th least significant
   * bits are used to offset into the PGD
   *
   * @return The page frame number (pfn) of the pmd if the page entry is marked
   * as in use, otherwise -1
   */
  int get_pmd(int pgd_pfn, unsigned int vaddr);

  /**
   * @brief Get the page table entries frame number (pte pfn)
   * @param pmd_pfn The page frame number of the page middle directory to offset
   * into
   * @param vaddr The virtual address, where the 14th-19th least significant
   * bits are used to offset into the PMD
   *
   * @return The page frame number (pfn) of the pte if the page entry is marked
   * as in use, otherwise -1
   */
  int get_pte(int pmd_pfn, unsigned int vaddr);

  /**
   * @brief Get the physical page frame number (phys pfn)
   * @param pte_pfn The page frame number of the page table entries to offset
   * into
   * @param vaddr The virtual address, where the 8th-13th least significant
   * bits are used to offset into the PTE
   *
   * @return The page frame number (pfn) of the physical page if the page entry
   * is marked as in use, otherwise -1
   */
  int get_phys(int pte_pfn, unsigned int vaddr);

  /**
   * @brief Get an entry given the pfn of a page table, and an offset
   * @param pfn The page frame number of the page to offset into
   * @param offset The offset into the page.
   * @return The page entry
   *
   * The 24 most significant bits of a page_entry refer to the PFN of another
   * page table. The 8 least significant bits of a page_entry are used as flags
   * for that page.
   */
  page_entry get_page_entry(int pfn, int offset);

  /**
   * @brief Set an entry given the pfn of a page table, and an offset
   * @param pfn The page frame number of the page to offset into
   * @param offset The offset into the page.
   * @param val The value to set the page entry, where the 24 most significant
   * bits correspond to the PFN of another page table, and the 8 least
   * significant bits are used as flags for the corresponding page table
   */
  void set_page_entry(int pfn, unsigned int offset, page_entry val);

  /**
   * @brief Perform a page walk and get a pointer to the location in physical
   * memory corresponding to the given virtual address
   *
   * @param pgd_addr The physical address of the page global directory
   * @param vaddr The virtual address being translated
   *
   * @return A pointer to the location in physical memory corresponding to the
   * given virtual address
   * @throws Runtime error if any of the pages are marked as not in use:
   */
  void *page_walk(unsigned int pgd_addr, unsigned int vaddr);
};

/**
 * @brief Used to store information related to a process
 */
struct proc_info {
  unsigned int pgd_addr;
  MMU *mmu;
};

/**
 * @brief Abstraction around a virtual address pointer, which automatically
 * performs pagewalk when dereferencing the object.
 */
template <typename T> class vaddr {
private:
  unsigned int virtual_address;
  proc_info *p;

public:
  vaddr(proc_info *process_info, unsigned int addr) {
    virtual_address = addr;
    p = process_info;
  }
  operator unsigned int() const { return virtual_address; }
  operator int() const { return virtual_address; }
  template <typename U> operator vaddr<U>() const {
    return vaddr<U>(p, virtual_address);
  }
  vaddr &operator+=(const int &rhs) {
    virtual_address += sizeof(T) * rhs;
    return *this;
  }
  friend vaddr operator+(vaddr lhs, const int &rhs) {
    lhs += rhs;
    return lhs;
  }
  vaddr &operator-=(const int &rhs) {
    virtual_address -= sizeof(T) * rhs;
    return *this;
  }
  friend vaddr operator-(vaddr lhs, const int &rhs) {
    lhs -= rhs;
    return lhs;
  }
  T &operator[](unsigned int index) {
    return *(T *)p->mmu->page_walk(p->pgd_addr,
                                   virtual_address + index * sizeof(T));
  }
  T &operator*() {
    return *(T *)p->mmu->page_walk(p->pgd_addr, virtual_address);
  }
  T *operator->() { return p->mmu->page_walk(p->pgd_addr, virtual_address); }
};

template <> class vaddr<void> {
private:
  unsigned int virtual_address;
  proc_info *p;

public:
  vaddr(proc_info *process_info, unsigned int addr) {
    virtual_address = addr;
    p = process_info;
  }
  operator unsigned int() const { return virtual_address; }
  operator int() const { return virtual_address; }
  template <typename U> operator vaddr<U>() const {
    return vaddr<U>(p, virtual_address);
  }
  void *operator->() { return p->mmu->page_walk(p->pgd_addr, virtual_address); }
};

#endif
