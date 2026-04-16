#include "my_mmu.h"
#include <cstdio>
#include <pthread.h>
#include <stdexcept>
#include <sys/mman.h>

MMU::MMU(unsigned int mem_size, unsigned int page_size) {
  physical_memory =
      mmap(nullptr, (mem_size / page_size) * page_size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  this->page_size = page_size;
  pthread_mutex_init(&free_pages_lock, NULL);

  for (unsigned int i = 0; i < (mem_size - 1) / page_size + 1; i++) {
    free_pages.push_back(i);
  }
}

unsigned int MMU::get_free_page() {
  pthread_mutex_lock(&free_pages_lock);
  unsigned int pfn = free_pages.front();
  free_pages.pop_front();
  pthread_mutex_unlock(&free_pages_lock);
  memset((char *)physical_memory + get_page(pfn), 0, page_size);
  return pfn;
}

unsigned int MMU::get_page(unsigned int pfn) { return page_size * pfn; }

void MMU::add_free_page(unsigned int pfn) {
  pthread_mutex_lock(&free_pages_lock);
  free_pages.push_back(pfn);
  pthread_mutex_unlock(&free_pages_lock);
}

int MMU::get_pmd(int pgd_pfn, unsigned int vaddr) {
  unsigned int pmd = get_page_entry(pgd_pfn, (vaddr >> 20) & 0x3F);
  return (pmd & 0x1) == 1 ? pmd >> 8 : -1;
}

int MMU::get_pte(int pmd_pfn, unsigned int vaddr) {
  unsigned int pte = get_page_entry(pmd_pfn, (vaddr >> 14) & 0x3F);
  return (pte & 0x1) == 1 ? pte >> 8 : -1;
}

int MMU::get_phys(int pte_pfn, unsigned int vaddr) {
  unsigned int phys = get_page_entry(pte_pfn, (vaddr >> 8) & 0x3F);
  return (phys & 0x1) == 1 ? phys >> 8 : -1;
}

unsigned int MMU::get_page_entry(int pfn, int offset) {
  return *(((int *)((char *)physical_memory + get_page(pfn))) + offset);
}

void MMU::set_page_entry(int pfn, unsigned int offset, unsigned int val) {
  *(((int *)((char *)physical_memory + get_page(pfn))) + offset) = val;
}

void *MMU::page_walk(unsigned int pgd_addr, unsigned int vaddr) {
  int pfn;
  if ((pfn = get_pmd(pgd_addr / page_size, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting pmd!");
    // return (void *)-1;
  }
  if ((pfn = get_pte(pfn, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting pte!");
    // return (void *)-1;
  }
  if ((pfn = get_phys(pfn, vaddr)) == -1) {
    throw std::runtime_error("Segfault when getting physical page!");
    // return (void *)-1;
  }
  return (((char *)physical_memory + get_page(pfn)) + (vaddr & 0xFF));
}

MMU::~MMU() {}
// vaddr<void> MMU::my_mmap(unsigned int pgd_addr, unsigned int length) {
//   unsigned int pfn = get_free_page();
//   return vaddr<void>(this, pgd_addr, pfn * page_size);
// }
