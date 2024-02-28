#include "InitRequest.h"

#include "InitResponse.h"
#include "CommandPacket.h"

InitRequest::InitRequest(const uint8_t request_sequence_number, const uint8_t device_id) : Request(request_sequence_number, CMD_INIT, device_id) {}

std::vector<uint8_t> InitRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
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
