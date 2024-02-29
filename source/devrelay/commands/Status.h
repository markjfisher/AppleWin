#pragma once

#include <cstdint>
#include <vector>

#include "../types/Request.h"
#include "../types/Response.h"

class StatusRequest : public Request
{
public:
	StatusRequest(uint8_t request_sequence_number, uint8_t device_id, uint8_t status_code);
	virtual std::vector<uint8_t> serialize() const override;
	std::unique_ptr<Response> deserialize(const std::vector<uint8_t> &data) const override;

	uint8_t get_status_code() const { return status_code_; }

private:
	uint8_t status_code_;
};


class StatusResponse : public Response
{
public:
	explicit StatusResponse(uint8_t request_sequence_number, uint8_t status);
	std::vector<uint8_t> serialize() const override;

	const std::vector<uint8_t> &get_data() const;
	void add_data(uint8_t d);
	void set_data(const std::vector<uint8_t> &data);

private:
	std::vector<uint8_t> data_;
};
