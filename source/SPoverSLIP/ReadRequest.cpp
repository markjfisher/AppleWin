#include "ReadRequest.h"

#include "ReadResponse.h"
#include "SmartPortCodes.h"

ReadRequest::ReadRequest(const uint8_t request_sequence_number, const uint8_t sp_unit)
    : Request(request_sequence_number, SP_READ, sp_unit), byte_count_(), address_()
{
}

std::vector<uint8_t> ReadRequest::serialize() const
{
  std::vector<uint8_t> request_data;
  request_data.push_back(this->get_request_sequence_number());
  request_data.push_back(this->get_command_number());
  request_data.push_back(this->get_sp_unit());
  request_data.insert(request_data.end(), get_byte_count().begin(), get_byte_count().end());
  request_data.insert(request_data.end(), get_address().begin(), get_address().end());
  return request_data;
}

std::unique_ptr<Response> ReadRequest::deserialize(const std::vector<uint8_t> &data) const
{
  if (data.size() < 3)
  {
    throw std::runtime_error("Not enough data to deserialize ReadResponse");
  }

  auto response = std::make_unique<ReadResponse>(data[0], data[1]);
  if (response->get_status() == 0)
  {
    response->set_data(data.begin() + 2, data.end());
  }
  return response;
}

const std::array<uint8_t, 2> &ReadRequest::get_byte_count() const { return byte_count_; }

const std::array<uint8_t, 3> &ReadRequest::get_address() const { return address_; }

void ReadRequest::set_byte_count_from_ptr(const uint8_t *ptr, const size_t offset) { std::copy_n(ptr + offset, byte_count_.size(), byte_count_.begin()); }

void ReadRequest::set_address_from_ptr(const uint8_t *ptr, const size_t offset) { std::copy_n(ptr + offset, address_.size(), address_.begin()); }
