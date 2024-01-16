#include "Connection.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <vector>

std::vector<uint8_t> Connection::wait_for_response(uint8_t request_id, std::chrono::seconds timeout)
{
  std::unique_lock<std::mutex> lock(responses_mutex_);
  // mutex is unlocked as it goes into a wait, so then the inserting thread can
  // add to map, and this can then pick it up when notified, or timeout.
  if (!response_cv_.wait_for(lock, timeout, [this, request_id]() { return responses_.count(request_id) > 0; }))
  {
    throw std::runtime_error("Timeout waiting for response");
  }
  std::vector<uint8_t> response_data = responses_[request_id];
  responses_.erase(request_id);
  return response_data;
}

std::vector<uint8_t> Connection::wait_for_request()
{
  std::unique_lock<std::mutex> lock(responses_mutex_);
  response_cv_.wait(lock, [this]() { return !responses_.empty(); });
  const auto it = responses_.begin();
  std::vector<uint8_t> request_data = it->second;
  responses_.erase(it);

  return request_data;
}

void Connection::join()
{
  if (reading_thread_.joinable())
  {
    reading_thread_.join();
  }
}
