#include "my_mmu.h"
#include "my_process.h"
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace std;

class TestProgram1 : public Program {
public:
  int temp = 0;
  int main(int argc, char *argv[]) {
    // mmap some pages
    vaddr<int> addrs = my_mmap(0, 1024);

    *addrs = 12345;

    // Should get 12345
    temp = *addrs;

    return 0;
  }
};

TEST(MMU, InitMMU) {
  MMU mmu = MMU(65536, 256);
  EXPECT_EQ(mmu.get_page_size(), 256);
  EXPECT_EQ(mmu.get_free_pages_size(), 256);
}

TEST(ProcessTest, Cleanup) {
  MMU mmu = MMU(65536, 256);
  {
    Process a = Process(progT<TestProgram1>{}, &mmu);
    char *args[] = {(char *)"program 1"};
    a.run(2, args);
    a.wait(NULL);
    // EXPECT_EQ(mmu.get_free_pages_size(), 249);
  }
  // Destructor for process gets called here
  EXPECT_EQ(mmu.get_free_pages_size(), 256);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
