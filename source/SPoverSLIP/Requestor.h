#pragma once

#include <memory>
#include "Connection.h"
#include "Listener.h"
#include "Request.h"
#include "Response.h"

class Requestor
{
public:
	Requestor()
	{
	}

	// The Request's deserialize function will always return a Response, e.g. StatusRequest -> StatusResponse
	// The request will be mutated 
	std::unique_ptr<Response> send_request(Request& request, Connection* connection);

private:
	Listener* listener_ = nullptr;
};
