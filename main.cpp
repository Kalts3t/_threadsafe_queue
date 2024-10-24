
#include <condition_variable>
#include <memory>
#include <thread>

template<typename T>
class queue_threadsafe {
private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::unique_ptr<node> head;
    std::mutex head_mutex;
    node* tail;
    std::mutex tail_mutex;
    std::condition_variable cv;
    std::condition_variable queue_not_full;
    std::atomic<size_t> current_size;
    size_t max_size;
    node* get_tail() {
        std::unique_lock<std::mutex> lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<T> pop_head() {
        std::unique_ptr<node> old_head(std::move(head));
        head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock lock(head_mutex);
        cv.wait(lock,[&]{return head.get()!=get_tail();});
        return lock;
    }
    std::unique_ptr<T> wait_pop_head() {
        std::unique_lock<std::mutex> lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<T> wait_pop_head(T& value) {
        std::unique_lock<std::mutex> lock(wait_for_data());
        value = std::move(*head->data);
        return pop_head();
    }

    std::unique_ptr<T> try_pop_head() {
        std::unique_lock<std::mutex> lock(head_mutex);
        if (head.get() == get_tail()) {
            return std::unique_ptr<T>();
        }
        return pop_head();
    }
    std::unique_ptr<T> try_pop_head(T& value) {
        std::unique_lock<std::mutex> lock(head_mutex);
        if (head.get() == get_tail()) {
            return std::unique_ptr<T>();
        }
        value = std::move(*head->data);
        return pop_head();
    }

    void check_capcity(std::unique_ptr<T> old_head) {
        if(old_head) {
            std::unique_lock<std::mutex> lock(tail_mutex);
            --current_size;
            queue_not_full.notify_one();
        }
    }
public:
    explicit queue_threadsafe(size_t max):max_size(max),current_size(0),head(new node),tail(head.get()){}
    queue_threadsafe(const queue_threadsafe& other)=delete;
    queue_threadsafe& operator=(const queue_threadsafe& other)=delete;
    std::unique_ptr<T> wait_for_pop() {
        std::unique_ptr<T> old_head = std::move(wait_pop_head());
        check_capcity(old_head);
        return old_head;
    }
    std::unique_ptr<T> wait_for_pop(T& value) {
        std::unique_ptr<T> old_head = std::move(wait_pop_head(value));
        value = std::move(*old_head->data);
        check_capcity(old_head);
        return old_head;
    }
    void push(T new_value) {
        {
            std::unique_ptr<T> new_data = std::make_unique<T>(std::move(new_value));
            std::unique_ptr<T> new_node(new node());
            std::unique_lock<std::mutex> lock(tail_mutex);
            queue_not_full.wait(lock,[&]{return max_size>current_size;});
            tail->data = std::move(new_data);
            tail->next = std::move(new_node);
            node* new_tail = new_node.get();
            tail = new_tail;
            ++current_size;
        }
        cv.notify_one();
    }

    std::unique_ptr<T> try_pop() {
        std::unique_ptr<T> old_head(try_pop_head());
        check_capcity(old_head);
        return old_head?old_head->data:std::unique_ptr<T>();
    }

    bool try_pop(T& value) {
        std::unique_ptr<T> old_head = std::move(try_pop_head(value));
        check_capcity(old_head);
        return old_head!=nullptr;
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(head_mutex);
        return head.get()==get_tail();
    }
};
