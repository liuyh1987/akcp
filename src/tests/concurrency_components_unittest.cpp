#include "../concurrency_components.h"
#include <gtest/gtest.h>

using namespace Akcp;

/*********************************************/
/************CountDownLatch*******************/
/*********************************************/
void ready(CountDownLatch& cdl, std::string resultname, int i)
{
    if (i % 2 == 0)
        cdl.countDown(resultname, true);
    else
        cdl.countDown();
}

TEST(CountDownLatch, All) {
    int tread_num = 10;
    CountDownLatch ctl(tread_num);
    EXPECT_EQ(ctl.getCount(), tread_num);
    std::string result_names[tread_num] = 
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
    std::shared_ptr<std::thread> spthreads[tread_num];
    for (int i=0; i<tread_num; ++i)
    {
        spthreads[i].reset(new std::thread(std::bind(&ready, std::ref(ctl), result_names[i], i)));
    }
    ctl.wait();

    for (auto& item : spthreads)
        item->join();
    EXPECT_TRUE(ctl.getResult("1"));
    EXPECT_FALSE(ctl.getResult("2"));
    EXPECT_EQ(ctl.getCount(), 0);
}

/*********************************************/
/************ExecutorTest*********************/
/*********************************************/
bool ExecutorTestSuccess() { return true; }
bool ExecutorTestFailed() { return false; }
bool ExecutorTestInAndOut(int in, int& out)
{
    out = ++in;
    return true;
}

TEST(ExecutorTest, Success) {
    Executor exec(std::bind(ExecutorTestSuccess));
    exec.start();
    bool result = exec.waitResult();
    EXPECT_TRUE(result);
}

TEST(ExecutorTest, Failed) {
    Executor exec(std::bind(ExecutorTestFailed));
    exec.start();
    bool result = exec.waitResult();
    EXPECT_FALSE(result);
}

TEST(ExecutorTest, InAndOut) {
    int in = 1;
    int out;
    Executor exec(std::bind(ExecutorTestInAndOut, in, std::ref(out)));
    exec.start();
    bool result = exec.waitResult();
    EXPECT_TRUE(result);
    EXPECT_EQ(out, ++in);
}

/*********************************************/
/************ThreadSafeQueue******************/
/*********************************************/
bool noBlockTakeOne(ThreadSafeQueue<int>& queue, int& total_take)
{
    sleep(1);
    int item;
    total_take = 0;
    while (queue.tryTakeOne(&item))
        total_take++;

    return true;
}

bool noBlockTakeAll(ThreadSafeQueue<int>& queue, int& total_take)
{
    sleep(1);
    int item;
    total_take = 0;
    ThreadSafeQueue<int>::QueueType queue_tmp;
    while (queue.tryTakeAll(&queue_tmp))
        total_take += queue_tmp.size();

    return true;
}

TEST(ThreadSafeQueueTest, NoBlockTakeOne) {
    ThreadSafeQueue<int> testqueu;
    int put_num = 1000;
    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    int excu1_take, excu2_take, excu3_take;
    Executor excu1(std::bind(noBlockTakeOne, std::ref(testqueu), std::ref(excu1_take)));
    Executor excu2(std::bind(noBlockTakeOne, std::ref(testqueu), std::ref(excu2_take)));
    excu1.start();
    excu2.start();

    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    Executor excu3(std::bind(noBlockTakeOne, std::ref(testqueu), std::ref(excu3_take)));
    excu3.start();
    EXPECT_TRUE(excu1.waitResult());
    EXPECT_TRUE(excu2.waitResult());
    EXPECT_TRUE(excu3.waitResult());
    EXPECT_EQ(put_num*2, excu1_take+excu2_take+excu3_take);
}

TEST(ThreadSafeQueueTest, NoBlockTakeAll) {
    ThreadSafeQueue<int> testqueu;
    int put_num = 10000;
    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    int excu1_take, excu2_take, excu3_take;
    Executor excu1(std::bind(noBlockTakeAll, std::ref(testqueu), std::ref(excu1_take)));
    Executor excu2(std::bind(noBlockTakeOne, std::ref(testqueu), std::ref(excu2_take)));
    excu1.start();
    excu2.start();

    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    Executor excu3(std::bind(noBlockTakeAll, std::ref(testqueu), std::ref(excu3_take)));
    excu3.start();
    EXPECT_TRUE(excu1.waitResult());
    EXPECT_TRUE(excu2.waitResult());
    EXPECT_TRUE(excu3.waitResult());
    EXPECT_EQ(put_num*2, excu1_take+excu2_take+excu3_take);
}

bool BlockTakeAll(ThreadSafeQueue<int>& queue, int& total_take)
{
    int item;
    total_take = 0;
    ThreadSafeQueue<int>::QueueType queue_tmp;
    while (queue.blockTakeAll(&queue_tmp))
        total_take += queue_tmp.size();

    return false;
}

TEST(ThreadSafeQueueTest, BlockTakeAllAndfinishBlock) {
    ThreadSafeQueue<int> testqueu;
    int put_num = 1000;
    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    int excu1_take, excu2_take, excu3_take;
    Executor excu1(std::bind(BlockTakeAll, std::ref(testqueu), std::ref(excu1_take)));
    Executor excu2(std::bind(BlockTakeAll, std::ref(testqueu), std::ref(excu2_take)));
    excu1.start();
    excu2.start();

    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    Executor excu3(std::bind(BlockTakeAll, std::ref(testqueu), std::ref(excu3_take)));
    excu3.start();

    sleep(1);
    testqueu.finishBlock();

    EXPECT_FALSE(excu1.waitResult());
    EXPECT_FALSE(excu2.waitResult());
    EXPECT_FALSE(excu3.waitResult());
    EXPECT_EQ(put_num*2, excu1_take+excu2_take+excu3_take);
}

bool BlockTakeOne(ThreadSafeQueue<int>& queue, int& total_take)
{
    int item;
    total_take = 0;
    ThreadSafeQueue<int>::QueueType queue_tmp;
    while (queue.blockTakeOne(&item))
        total_take ++;

    return false;
}

TEST(ThreadSafeQueueTest, BlockTakeOneAndfinishBlock) {
    ThreadSafeQueue<int> testqueu;
    int put_num = 1000;
    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    int excu1_take, excu2_take, excu3_take;
    Executor excu1(std::bind(BlockTakeOne, std::ref(testqueu), std::ref(excu1_take)));
    Executor excu2(std::bind(BlockTakeOne, std::ref(testqueu), std::ref(excu2_take)));
    excu1.start();
    excu2.start();

    for (int i=0; i<put_num; ++i)
        testqueu.put(i);

    Executor excu3(std::bind(BlockTakeOne, std::ref(testqueu), std::ref(excu3_take)));
    excu3.start();

    sleep(1);
    testqueu.finishBlock();

    EXPECT_FALSE(excu1.waitResult());
    EXPECT_FALSE(excu2.waitResult());
    EXPECT_FALSE(excu3.waitResult());
    EXPECT_EQ(put_num*2, excu1_take+excu2_take+excu3_take);
}

TEST(ThreadSafeQueueTest, size) {
    ThreadSafeQueue<int> testqueu;
    EXPECT_EQ(testqueu.size(), 0);
    int put_num = 1000;
    for (int i=0; i<put_num; ++i)
        testqueu.put(i);
    EXPECT_EQ(testqueu.size(), put_num);
    int take_num = 500;
    int item;
    for (int i=0; i<take_num; ++i)
        EXPECT_TRUE(testqueu.tryTakeOne(&item));
    EXPECT_EQ(testqueu.size(), put_num-take_num);
    ThreadSafeQueue<int>::QueueType tmp_queue;
    EXPECT_TRUE(testqueu.tryTakeAll(&tmp_queue));
    EXPECT_EQ(testqueu.size(), 0);
}

