#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

template<typename T>
class threadsafe_queue {
    private:
    mutable std::mutex m;
    std::condition_variable cv;
    std::queue<T> queue_;
    public:
    threadsafe_queue()=default;
    threadsafe_queue& operator=(const threadsafe_queue& rhs)=delete;
    threadsafe_queue(const threadsafe_queue& rhs)=delete;

    threadsafe_queue(threadsafe_queue& other) {
        std::unique_lock<std::mutex> lock(m);
        queue_=other.queue_;
    }

    void push(T item) {
        std::unique_lock<std::mutex> lock(m);
        queue_.push(item);
        cv.notify_one();
    }

    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(m);
        wait(lock, [&]{ return !queue_.empty(); });
        auto item = queue_.front();
        queue_.pop();
        return item;
    }

    std::shared_ptr<T> shared_wait_pop() {
        std::unique_lock<std::mutex> lock(m);
        wait(lock, [&]{ return !queue_.empty(); });
        std::shared_ptr<T> item(make_shared<T>(queue_.front()));
        queue_.pop();
        return item;
    }

    bool empty() const {
        std::unique_lock<std::mutex> lock(m);
        return queue_.empty();
    }
};
