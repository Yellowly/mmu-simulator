#include <gtest/gtest.h>
#include <scheduling.h>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

TEST(SchedulingTest, ReadWorkload1) {
    pqueue_arrival pq = read_workload("workloads/workload_01.txt");
    EXPECT_EQ(pq.size(), 3) << "workload_01 should load 3 processes";
}

TEST(SchedulingTest, FIFO1) {
    pqueue_arrival pq = read_workload("workloads/workload_01.txt");
    list<Process> xs = fifo(pq);
    float t = avg_turnaround(xs);
    float r = avg_response(xs);
    EXPECT_FLOAT_EQ(t, 20) << "FIFO workload_01 turnaround expected 20";
    EXPECT_FLOAT_EQ(r, 10) << "FIFO workload_01 response expected 10";
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
