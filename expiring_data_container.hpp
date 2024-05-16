#ifndef EXPIRING_DATA_CONTAINER_HPP
#define EXPIRING_DATA_CONTAINER_HPP

#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <iostream>

/**
 * @brief A data structure that holds data with a fixed duration.
 *
 * The ExpiringDataContainer allows inserting data elements with an associated
 * insertion time. Each data element stays in the structure for a fixed duration
 * and is automatically removed after that time has elapsed.
 *
 * Multiple threads are used to manage data insertion, removal of expired data,
 * and retrieval of valid data. A cleanup thread periodically removes expired
 * data elements. Mutexes are used to ensure thread safety when accessing shared
 * data structures such as the priority queue holding the data elements. Conditional
 * variables are used to notify the cleanup thread when new data is inserted,
 * allowing it to efficiently wait for new data without busy waiting.
 */
template <typename T>
class ExpiringDataContainer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

    struct TimedData {
        T data;
        TimePoint insertion;
        TimePoint expiration;

        bool operator>(const TimedData& other) const {
            return expiration > other.expiration;
        }
    };

    explicit ExpiringDataContainer(Duration duration);
    ~ExpiringDataContainer();

    void insert(const T& data);
    std::vector<T> get_valid_data();
    bool is_less_than_all(TimePoint time);
    std::vector<T> get_data_exceeding(TimePoint time);
    void print_state();

private:
    std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>> pq;
    Duration fixed_duration;
    std::thread cleanup_thread;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_cleanup = false;

    void cleanup_expired_data();
    void remove_expired(TimePoint now);
};

#include "expiring_data_container.tpp"

#endif // EXPIRING_DATA_CONTAINER_HPP
