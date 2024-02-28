#include "FormatRequest.h"

#include "FormatResponse.h"
#include "CommandPacket.h"

FormatRequest::FormatRequest(const uint8_t request_sequence_number, const uint8_t device_id) : Request(request_sequence_number, CMD_FORMAT, device_id) {}

std::vector<uint8_t> FormatRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
	return request_data;
}

std::unique_ptr<Response> FormatRequest::deserialize(const std::vector<uint8_t> &data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize FormatResponse");
	}

	auto response = std::make_unique<FormatResponse>(data[0], data[1]);
	return response;
}
