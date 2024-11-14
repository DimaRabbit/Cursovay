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
    bool stopped = false; // Флаг остановки

public:
    // Метод для добавления задачи в очередь
    void push(T task) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(task);
        cv.notify_one();  // Уведомляем один поток
    }

    // Метод для извлечения задачи из очереди
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // Ожидаем, пока очередь не станет пустой, или пока не будет установлен флаг остановки
        cv.wait(lock, [this] { return !queue.empty() || stopped; });

        if (queue.empty()) return nullptr;  // Если очередь пуста, возвращаем nullptr (ничего не делать)

        T task = queue.front();
        queue.pop();
        return task;
    }

    // Метод для установки флага остановки
    void stop() {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = true;
        cv.notify_all();  // Уведомляем все потоки о завершении
    }

    // Метод для проверки, пуста ли очередь
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

class thread_pool {
private:
    std::vector<std::thread> workers;
    safe_queue<std::function<void()>> task_queue;
    std::atomic<bool> stop; // Флаг для остановки работы потоков

public:
    // Конструктор пула потоков
    thread_pool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.push_back(std::thread(&thread_pool::work, this));
        }
    }

    // Метод для выполнения работы в потоке
    void work() {
        while (!stop) {
            std::function<void()> task = task_queue.pop(); // Получаем задачу из очереди

            if (task) {
                task(); // Выполняем задачу
            }
        }
    }

    // Метод для добавления задачи в очередь
    template <typename F>
    void submit(F&& task) {
        task_queue.push(std::forward<F>(task));
    }

    // Метод для остановки всех потоков
    void shutdown() {
        stop = true;  // Устанавливаем флаг завершения работы
        task_queue.stop();  // Останавливаем очередь и уведомляем все потоки

        // Ожидаем завершения всех потоков
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Деструктор, вызывающий shutdown
    ~thread_pool() {
        shutdown();
    }
};

// Тестовые функции
void test_task_1() {
    std::cout << "Task 1 executed\n";
}

void test_task_2() {
    std::cout << "Task 2 executed\n";
}

int main() {
    size_t num_threads = std::thread::hardware_concurrency(); // Определяем количество потоков на основе количества ядер
    thread_pool pool(num_threads); // Создаем пул потоков

    // Добавляем задачи в пул
    pool.submit(test_task_1);
    pool.submit(test_task_2);

    std::this_thread::sleep_for(std::chrono::seconds(1)); // Ожидаем выполнения задач

    pool.shutdown(); // Завершаем работу пула

    return 0;
}