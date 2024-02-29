#include <stdexcept>

#include "Status.h"

StatusRequest::StatusRequest(const uint8_t request_sequence_number, const uint8_t device_id, const uint8_t status_code) : Request(request_sequence_number, CMD_STATUS, device_id), status_code_(status_code)
{
}

std::vector<uint8_t> StatusRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
	request_data.push_back(this->get_status_code());

	return request_data;
}

std::unique_ptr<Response> StatusRequest::deserialize(const std::vector<uint8_t> &data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize StatusResponse");
	}

	auto response = std::make_unique<StatusResponse>(data[0], data[1]);

	if (response->get_status() == 0 && data.size() > 2)
	{
		for (size_t i = 2; i < data.size(); ++i)
		{
			response->add_data(data[i]);
		}
	}

	return response;
}

StatusResponse::StatusResponse(const uint8_t request_sequence_number, const uint8_t status) : Response(request_sequence_number, status) {}

const std::vector<uint8_t> &StatusResponse::get_data() const { return data_; }

void StatusResponse::add_data(const uint8_t d) { data_.push_back(d); }

void StatusResponse::set_data(const std::vector<uint8_t> &data) { data_ = data; }

std::vector<uint8_t> StatusResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());

	for (uint8_t b : get_data())
	{
		data.push_back(b);
	}
	return data;
}
