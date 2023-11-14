#include "StdAfx.h"

#include <iostream>
#include "Requestor.h"
#include "Listener.h"

std::unique_ptr<Response> Requestor::send_request(Request& request, Connection* connection)
{
#ifdef DEBUG
  std::cout << "Requestor::send_request, request: " << request << std::endl;
#endif

	auto device_id = request.get_sp_unit();

	// The request's device_id given needs to map back to the unit_id of the target
	auto unit_id = connection->get_unit_id_by_device_index(device_id);
	request.set_sp_unit(unit_id);

	// Send the serialized request
	connection->send_data(request.serialize());

	std::vector<uint8_t> response_data;
	try
	{
		response_data = connection->wait_for_response(request.get_request_sequence_number(), std::chrono::seconds(20));
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "Requestor::send_request did not get response, error = " << e.what() << std::endl;
		return nullptr;
	}

	// Deserialize the response data into a Response object.
	// Each Request type (e.g. StatusRequest) is able to deserialize into its twin Response (e.g. StatusResponse).
	return request.deserialize(response_data);
}
