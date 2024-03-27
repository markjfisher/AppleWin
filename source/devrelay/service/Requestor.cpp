#ifdef DEV_RELAY_SLIP

#include <iostream>

#include "Requestor.h"
#include "Listener.h"

uint8_t Requestor::request_number_ = 0;

Requestor::Requestor() = default;

std::unique_ptr<Response> Requestor::send_request(const Request &request, Connection *connection)
{
	// Send the serialized request
	connection->send_data(request.serialize());

	std::optional<std::vector<uint8_t>> response_data = connection->wait_for_response(request.get_request_sequence_number(), std::chrono::seconds(GetCommandListener().get_response_timeout()));
	if (!response_data)
	{
		std::cerr << "Requestor::send_request Timeout waiting for response" << std::endl;
		return nullptr;
	}

	// Deserialize the response data into a Response object.
	// Each Request type (e.g. StatusRequest) is able to deserialize into its twin Response (e.g. StatusResponse).
	return request.deserialize(*response_data);
}

uint8_t Requestor::next_request_number()
{
	const uint8_t current_number = request_number_;
	request_number_ = (request_number_ + 1) % 256;
	return current_number;
}

#endif
