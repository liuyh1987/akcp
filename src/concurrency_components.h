#ifndef THREAD_SAFE_QUEUE_H_
#define THREAD_SAFE_QUEUE_H_

#include <deque>
#include <map>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <chrono>
#include <iostream>

namespace Akcp{

class noncopyable
{
protected:
    noncopyable(){}
    ~noncopyable(){}
    noncopyable(const noncopyable&) = delete;
    const noncopyable& operator=(const noncopyable&) = delete;
};

class CountDownLatch: noncopyable
{
public:
    explicit CountDownLatch(int count)
        :count_(count)
    {
        assert(count_ > 0);
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ != 0)
        {
            cond_.wait(lock);
        }
    }

    bool waitFor(std::chrono::milliseconds msecs)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ != 0)
        {
            std::cv_status ret = cond_.wait_for(lock, msecs);
            if (ret == std::cv_status::timeout)
                break;
        }

        return count_ == 0;
    }

    void countDown()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_--;
        cond_.notify_all();
    }

    void countDown(const std::string& resultname, bool result)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_--;
        results_[resultname] = result;
        cond_.notify_all();
    }

    int getCount() const
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return count_;
    }

    bool getResult(const std::string& resultname)
    {
        std::map<std::string, bool>::iterator iter = 
            results_.find(resultname);
        if (iter == results_.end())
            return false;
        return iter->second;
    }

private:
    mutable std::mutex mutex_;
    int count_;
    std::map<std::string, bool> results_;
    std::condition_variable cond_;
};

class Executor: noncopyable
{
public:
    typedef std::function<bool ()> FnCallback;

    Executor(FnCallback fn)
        :cb_(fn),
         finished_(false)
    { }

    ~Executor()
    {
        if (spt_->joinable())
            spt_->join();
    }

    void start()
    {
        spt_.reset(new std::thread(std::bind(&Executor::run, this)));
    }

    void run()
    {
        result_ = cb_();
        std::unique_lock<std::mutex> lock(mutex_);
        finished_ = true;
        cond_finished_.notify_one();
    }

    bool waitResult()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!finished_)
        {
            cond_finished_.wait(lock);
        }

        return result_;
    }


private:
    FnCallback cb_;
    std::shared_ptr<std::thread> spt_;
    bool result_;

    std::mutex mutex_;
    bool finished_;
    std::condition_variable cond_finished_;
};

template<typename T>
class ThreadSafeQueue: noncopyable
{
public:
    typedef std::deque<T> QueueType;

    ThreadSafeQueue() { } 

    //noblock
    void put(const T& x)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push_back(x);
    }

    bool tryTakeOne(T* item)
    {
        assert(item != NULL);
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
        *item = queue_.front();
        queue_.pop_front();
        return true;
    }

    bool tryTakeAll(QueueType* queue)
    {
        assert(queue != NULL);
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
        swap(*queue, queue_);
        return true;
    }

    //block
    void putWithNotify(const T& x)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push_back(x);
        notEmpty_.notify_one(); 
    }

    bool blockTakeOne(T* item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        block_ = true;
        while (block_ && queue_.empty())
        {
            notEmpty_.wait(lock);
        }
        if (!block_)
            return false;

        assert(!queue_.empty());
        *item = queue_.front();
        queue_.pop_front();
        return true;
    }

    bool blockTakeAll(QueueType* queue)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        block_ = true;
        while (block_  && queue_.empty())
        {
            notEmpty_.wait(lock);
        }
        if (!block_)
            return false;
        assert(!queue_.empty());
        swap(queue_, *queue);
        return true;
    }

    void finishBlock()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        block_ = false;
        notEmpty_.notify_all(); 
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::deque<T> queue_;
    bool block_;
};

template<typename T>
class QueueConsumer: noncopyable
{
public:
    typedef std::function<void (const T& item)> FnProcessor;
    typedef ThreadSafeQueue<T> ThreadSafeQueueType;

    QueueConsumer(FnProcessor processor, int threadnum = 1)
        :runing_(true),
         processorr_(processor)
    {
        try
        {
            for (int i = 0; i < threadnum ; ++i)
            {
                SpThread thread(new std::thread(std::bind(&QueueConsumer<T>::ThreadLoop, this)));
                threads_.push_back(thread);
            }
        }
        catch (std::exception& e)
        {
            std::cout << "exception" << std::endl;
            std::cout << e.what() << std::endl;
        }
        catch (...)
        {

            std::cout << "...." << std::endl;
        }
    }

    void put(const T& x)
    {
        queue_.putWithNotify(x);
    }

    void stop()
    {
        if (!runing_)
            return;
        
        runing_ = false;
        queue_.finishBlock();
        for (size_t i = 0; i<threads_.size(); ++i)
            if (threads_[i]->joinable())
                threads_[i]->join();
    }

    ~QueueConsumer()
    {
        stop();
    }

    ThreadSafeQueueType& threadSafeQueue()   
    {
        return queue_;
    }

private:
    void ThreadLoop()
    {
        while (runing_)
        {
            assert(threads_.size() > 0);
            if (threads_.size() > 1)
            {
                T item;
                if (!queue_.blockTakeOne(&item))
                    continue;
                processorr_(item);
            }
            else
            {
                typename ThreadSafeQueue<T>::QueueType queue;
                if (!queue_.blockTakeAll(&queue))
                    continue;

                for (typename ThreadSafeQueue<T>::QueueType::iterator iter = queue.begin();
                        iter!=queue.end(); ++iter)
                {
                    processorr_(*iter);
                }
            }
        }
    }

private:
    typedef std::shared_ptr<std::thread> SpThread;
    std::vector<SpThread> threads_;
    FnProcessor processorr_;
    ThreadSafeQueue<T> queue_;
    volatile bool runing_;
};
}
#endif//THREAD_SAFE_QUEUE_H_