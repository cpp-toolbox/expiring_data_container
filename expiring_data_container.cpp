#include "expiring_data_container.hpp"

ExpiringDataContainer::ExpiringDataContainer(Duration duration)
    : fixed_duration(duration) {
  cleanup_thread =
      std::thread(&ExpiringDataContainer::cleanup_expired_data, this);
}

ExpiringDataContainer::~ExpiringDataContainer() {
  {
    std::lock_guard<std::mutex> lock(mtx);
    stop_cleanup = true;
  }
  cv.notify_all();
  cleanup_thread.join();
}

void ExpiringDataContainer::insert(int data) {
  TimePoint now = Clock::now();
  TimePoint expiration = now + fixed_duration;
  {
    std::lock_guard<std::mutex> lock(mtx);
    pq.push({data, now, expiration});
  }
  cv.notify_all();
}

std::vector<int> ExpiringDataContainer::get_valid_data() {
  std::lock_guard<std::mutex> lock(mtx);
  auto now = Clock::now();
  remove_expired(now);
  std::vector<int> valid_data;
  std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>>
      temp_pq = pq;
  while (!temp_pq.empty()) {
    valid_data.push_back(temp_pq.top().data);
    temp_pq.pop();
  }
  return valid_data;
}

bool ExpiringDataContainer::is_less_than_all(TimePoint time) {
  std::lock_guard<std::mutex> lock(mtx);
  std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>>
      temp_pq = pq;
  while (!temp_pq.empty()) {
    if (temp_pq.top().insertion <= time) {
      return false;
    }
    temp_pq.pop();
  }
  return true;
}

std::vector<int> ExpiringDataContainer::get_data_exceeding(TimePoint time) {
  std::lock_guard<std::mutex> lock(mtx);
  auto now = Clock::now();
  remove_expired(now);
  std::vector<int> exceeding_data;
  std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>>
      temp_pq = pq;
  while (!temp_pq.empty()) {
    if (temp_pq.top().insertion > time) {
      exceeding_data.push_back(temp_pq.top().data);
    }
    temp_pq.pop();
  }
  return exceeding_data;
}

void ExpiringDataContainer::print_state() {
  std::lock_guard<std::mutex> lock(mtx);
  std::priority_queue<TimedData, std::vector<TimedData>, std::greater<>>
      temp_pq = pq;
  std::cout << "State of the data ordered by insertion times:" << std::endl;
  while (!temp_pq.empty()) {
    TimePoint now = Clock::now();
    auto duration_in_data_structure =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - temp_pq.top().insertion);
    auto expiration_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            temp_pq.top().expiration.time_since_epoch());
    std::cout << "Data: " << temp_pq.top().data << ", Insertion Time: "
              << temp_pq.top().insertion.time_since_epoch().count() << " ms"
              << ", Expiration Time: " << expiration_time.count() << " ms"
              << ", Duration in Data Structure: "
              << duration_in_data_structure.count() << " ms" << std::endl;
    temp_pq.pop();
  }
}

void ExpiringDataContainer::cleanup_expired_data() {
  while (true) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, fixed_duration, [this] { return stop_cleanup; });
    if (stop_cleanup)
      break;
    auto now = Clock::now();
    remove_expired(now);
  }
}

void ExpiringDataContainer::remove_expired(TimePoint now) {
  while (!pq.empty() && pq.top().expiration <= now) {
    pq.pop();
  }
}
