// ReSharper disable CppPassValueParameterByConstReference

#include "WriteRequest.h"

#include "SmartPortCodes.h"
#include "WriteResponse.h"

WriteRequest::WriteRequest(const uint8_t request_sequence_number, const uint8_t sp_unit) : Request(request_sequence_number, SP_WRITE, sp_unit), byte_count_(), address_() {}

std::vector<uint8_t> WriteRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_sp_unit());
	request_data.insert(request_data.end(), get_byte_count().begin(), get_byte_count().end());
	request_data.insert(request_data.end(), get_address().begin(), get_address().end());
	request_data.insert(request_data.end(), get_data().begin(), get_data().end());
	return request_data;
}

std::unique_ptr<Response> WriteRequest::deserialize(const std::vector<uint8_t> &data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize WriteResponse");
	}

	auto response = std::make_unique<WriteResponse>(data[0], data[1]);
	return response;
}

const std::array<uint8_t, 2> &WriteRequest::get_byte_count() const { return byte_count_; }

const std::array<uint8_t, 3> &WriteRequest::get_address() const { return address_; }

void WriteRequest::set_byte_count_from_ptr(const uint8_t *ptr, const size_t offset) { std::copy_n(ptr + offset, byte_count_.size(), byte_count_.begin()); }

void WriteRequest::set_address_from_ptr(const uint8_t *ptr, const size_t offset) { std::copy_n(ptr + offset, address_.size(), address_.begin()); }

void WriteRequest::set_data_from_ptr(const uint8_t *ptr, const size_t offset, const size_t length)
{
	data_.clear();
	data_.insert(data_.end(), ptr + offset, ptr + offset + length);
}