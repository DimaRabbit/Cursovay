#include <vector>
#include <thread>
#include <functional>
#include <iostream>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <queue>

template <typename T>
class safe_queue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopped = false; // ���� ���������

public:
    // ����� ��� ���������� ������ � �������
    void push(T task) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(task);
        cv.notify_one();  // ���������� ���� �����
    }

    // ����� ��� ���������� ������ �� �������
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // �������, ���� ������� �� ������ ������, ��� ���� �� ����� ���������� ���� ���������
        cv.wait(lock, [this] { return !queue.empty() || stopped; });

        if (queue.empty()) return nullptr;  // ���� ������� �����, ���������� nullptr (������ �� ������)

        T task = queue.front();
        queue.pop();
        return task;
    }

    // ����� ��� ��������� ����� ���������
    void stop() {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = true;
        cv.notify_all();  // ���������� ��� ������ � ����������
    }

    // ����� ��� ��������, ����� �� �������
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

class thread_pool {
private:
    std::vector<std::thread> workers;
    safe_queue<std::function<void()>> task_queue;
    std::atomic<bool> stop; // ���� ��� ��������� ������ �������

public:
    // ����������� ���� �������
    thread_pool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.push_back(std::thread(&thread_pool::work, this));
        }
    }

    // ����� ��� ���������� ������ � ������
    void work() {
        while (!stop) {
            std::function<void()> task = task_queue.pop(); // �������� ������ �� �������

            if (task) {
                task(); // ��������� ������
            }
        }
    }

    // ����� ��� ���������� ������ � �������
    template <typename F>
    void submit(F&& task) {
        task_queue.push(std::forward<F>(task));
    }

    // ����� ��� ��������� ���� �������
    void shutdown() {
        stop = true;  // ������������� ���� ���������� ������
        task_queue.stop();  // ������������� ������� � ���������� ��� ������

        // ������� ���������� ���� �������
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // ����������, ���������� shutdown
    ~thread_pool() {
        shutdown();
    }
};

// �������� �������
void test_task_1() {
    std::cout << "Task 1 executed\n";
}

void test_task_2() {
    std::cout << "Task 2 executed\n";
}

int main() {
    size_t num_threads = std::thread::hardware_concurrency(); // ���������� ���������� ������� �� ������ ���������� ����
    thread_pool pool(num_threads); // ������� ��� �������

    // ��������� ������ � ���
    pool.submit(test_task_1);
    pool.submit(test_task_2);

    std::this_thread::sleep_for(std::chrono::seconds(1)); // ������� ���������� �����

    pool.shutdown(); // ��������� ������ ����

    return 0;
}