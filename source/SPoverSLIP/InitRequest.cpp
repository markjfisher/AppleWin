#include "InitRequest.h"

#include "InitResponse.h"
#include "SmartPortCodes.h"

InitRequest::InitRequest(const uint8_t request_sequence_number, const uint8_t sp_unit) : Request(request_sequence_number, SP_INIT, sp_unit) {}

std::vector<uint8_t> InitRequest::serialize() const
{
  std::vector<uint8_t> request_data;
  request_data.push_back(this->get_request_sequence_number());
  request_data.push_back(this->get_command_number());
  request_data.push_back(this->get_sp_unit());
  return request_data;
}

std::unique_ptr<Response> InitRequest::deserialize(const std::vector<uint8_t> &data) const
{
  if (data.size() < 2)
  {
    throw std::runtime_error("Not enough data to deserialize InitResponse");
  }

  auto response = std::make_unique<InitResponse>(data[0], data[1]);
  return response;
}
