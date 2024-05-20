#include "expiring_data_container.hpp"

template <typename T>
ExpiringDataContainer<T>::ExpiringDataContainer(Duration duration) : fixed_duration(duration) {
    cleanup_thread = std::thread(&ExpiringDataContainer::cleanup_expired_data, this);
}

template <typename T>
ExpiringDataContainer<T>::~ExpiringDataContainer() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop_cleanup = true;
    }
    cv.notify_all();
    cleanup_thread.join();
}

template <typename T>
void ExpiringDataContainer<T>::insert(const T& data) {
    TimePoint now = Clock::now();
    TimePoint expiration = now + fixed_duration;
    {
        std::lock_guard<std::mutex> lock(mtx);
        TimedData timed_data = {data, now, expiration};
        pq.push(timed_data);
        insertion_order.push_back(timed_data);
    }
    cv.notify_all();
}

template <typename T>
std::vector<T> ExpiringDataContainer<T>::get_valid_data() {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = Clock::now();
    remove_expired(now);
    std::vector<T> valid_data;
    std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>> temp_pq = pq;
    while (!temp_pq.empty()) {
        valid_data.push_back(temp_pq.top().data);
        temp_pq.pop();
    }
    return valid_data;
}

template <typename T>
bool ExpiringDataContainer<T>::is_less_than_all(TimePoint time) const {
    std::lock_guard<std::mutex> lock(mtx);
    std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>> temp_pq = pq;
    while (!temp_pq.empty()) {
        if (temp_pq.top().insertion <= time) {
            return false;
        }
        temp_pq.pop();
    }
    return true;
}

template <typename T>
std::vector<T> ExpiringDataContainer<T>::get_data_exceeding(TimePoint time) {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = Clock::now();
    remove_expired(now);
    std::vector<T> exceeding_data;
    std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>> temp_pq = pq;
    while (!temp_pq.empty()) {
        if (temp_pq.top().insertion > time) {
            exceeding_data.push_back(temp_pq.top().data);
        }
        temp_pq.pop();
    }
    return exceeding_data;
}

template <typename T>
void ExpiringDataContainer<T>::print_state() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>> temp_pq = pq;
    std::cout << "State of the data ordered by insertion times:" << std::endl;
    while (!temp_pq.empty()) {
        TimePoint now = Clock::now();
        auto duration_in_data_structure = std::chrono::duration_cast<std::chrono::milliseconds>(now - temp_pq.top().insertion);
        auto expiration_time = std::chrono::duration_cast<std::chrono::milliseconds>(temp_pq.top().expiration.time_since_epoch());

        //std::cout << "Data: " << temp_pq.top().data 
        //          << ", Insertion Time: " << temp_pq.top().insertion.time_since_epoch().count() << " ms"
        //          << ", Expiration Time: " << expiration_time.count() << " ms"
        //          << ", Duration in Data Structure: " << duration_in_data_structure.count() << " ms" << std::endl;

        std::cout << ", Insertion Time: " << temp_pq.top().insertion.time_since_epoch().count() << " ms";

        temp_pq.pop();
    }
}

template <typename T>
void ExpiringDataContainer<T>::cleanup_expired_data() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, fixed_duration, [this] { return stop_cleanup; });
        if (stop_cleanup) break;
        auto now = Clock::now();
        remove_expired(now);
    }
}

template <typename T>
void ExpiringDataContainer<T>::remove_expired(TimePoint now) {
    while (!pq.empty() && pq.top().expiration <= now) {
        pq.pop();
    }
    while (!insertion_order.empty() && insertion_order.front().expiration <= now) {
        insertion_order.pop_front();
    }
}

template <typename T>
size_t ExpiringDataContainer<T>::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return pq.size();
}

template <typename T>
T ExpiringDataContainer<T>::get_most_recent() const {
    std::lock_guard<std::mutex> lock(mtx);
    if (insertion_order.empty()) {
        throw std::runtime_error("No elements in the container");
    }
    return insertion_order.back().data;
}
