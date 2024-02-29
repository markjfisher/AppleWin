#include "Close.h"

CloseRequest::CloseRequest(const uint8_t request_sequence_number, const uint8_t device_id) : Request(request_sequence_number, CMD_CLOSE, device_id) {}

std::vector<uint8_t> CloseRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
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

CloseResponse::CloseResponse(const uint8_t request_sequence_number, const uint8_t status) : Response(request_sequence_number, status) {}

std::vector<uint8_t> CloseResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());
	return data;
}