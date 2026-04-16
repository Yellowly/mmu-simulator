# mmu-simulator
Basic simulation of the Memory Mangement Unit. 

## Features
- Free pages list - [ref](https://github.com/Yellowly/mmu-simulator/blob/main/src/my_mmu.cpp)
- Pagewalk - [ref](https://github.com/Yellowly/mmu-simulator/blob/main/src/my_mmu.cpp)
- mmap / munmap - [ref](https://github.com/Yellowly/mmu-simulator/blob/main/src/process_manager.cpp)
- Virtual address pointers - [ref](https://github.com/Yellowly/mmu-simulator/blob/main/include/my_mmu.h)
- Process abstraction for interacting with the simulated mmu - [ref](https://github.com/Yellowly/mmu-simulator/blob/main/src/programs.cpp)

## Details
- "Physical memory" is abstracted as a large continguous mmap'd region
- Processes are abstracted as threads
- "Programs" inherit from a superclass which provides access to `mmap` and `munmap`
