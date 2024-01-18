#include "stdafx.h"
#include "CloseRequest.h"

#include "CloseResponse.h"
#include "SmartPortCodes.h"

CloseRequest::CloseRequest(const uint8_t request_sequence_number, const uint8_t sp_unit) : Request(request_sequence_number, SP_CLOSE, sp_unit) {}

std::vector<uint8_t> CloseRequest::serialize() const
{
  std::vector<uint8_t> request_data;
  request_data.push_back(this->get_request_sequence_number());
  request_data.push_back(this->get_command_number());
  request_data.push_back(this->get_sp_unit());
  return request_data;
}

std::unique_ptr<Response> CloseRequest::deserialize(const std::vector<uint8_t> &data) const
{
  if (data.size() < 2)
  {
    throw std::runtime_error("Not enough data to deserialize CloseResponse");
  }

  auto response = std::make_unique<CloseResponse>(data[0], data[1]);
  return response;
}
