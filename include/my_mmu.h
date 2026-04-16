#ifndef __MY_MMU_H
#define __MY_MMU_H
#include <cstring>
#include <list>
#include <pthread.h>
// #define PAGE_SIZE 256

// template <unsigned int page_size = 256, unsigned int num_pages = 65536>

class MMU {
private:
  void *physical_memory;
  std::list<unsigned int> free_pages;
  unsigned int page_size;
  pthread_mutex_t free_pages_lock;

public:
  MMU(unsigned int mem_size, unsigned int page_size);

  unsigned int get_page_size() { return page_size; }

  unsigned int get_free_page();
  unsigned int get_page(unsigned int pfn);
  void add_free_page(unsigned int pfn);
  int get_pmd(int pgd_pfn, unsigned int vaddr);
  int get_pte(int pmd_pfn, unsigned int vaddr);
  int get_phys(int pte_pfn, unsigned int vaddr);
  unsigned int get_page_entry(int pfn, int offset);
  void set_page_entry(int pfn, unsigned int offset, unsigned int val);
  void *page_walk(unsigned int pgd_addr, unsigned int vaddr);
  ~MMU();
};

struct proc_info {
  unsigned int pgd_addr;
  MMU *mmu;
};

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
    return p->mmu->page_walk(p->pgd_addr, virtual_address + index);
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
