
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Listener.h"

#include "InitRequest.h"
#include "InitResponse.h"
#include "SLIP.h"
#include "TCPConnection.h"

#include "Log.h"
#include "Requestor.h"

// clang-format off
#ifdef WIN32
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
  #define CLOSE_SOCKET closesocket
  #define SOCKET_ERROR_CODE WSAGetLastError()
#else
  #include <errno.h>
  #define CLOSE_SOCKET close
  #define SOCKET_ERROR_CODE errno
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
#endif
// clang-format off

uint8_t Listener::next_device_id_ = 1;

Listener::Listener(std::string ip_address, const u_short port)
    : ip_address_(std::move(ip_address)), port_(port), is_listening_(false)
{
}

bool Listener::get_is_listening() const { return is_listening_; }

void Listener::insert_connection(uint8_t start_id, uint8_t end_id, const std::shared_ptr<Connection> &conn)
{
  connection_map_[{start_id, end_id}] = conn;
}

uint8_t Listener::get_total_device_count() { return next_device_id_ - 1; }

Listener::~Listener() { stop(); }

std::thread Listener::create_listener_thread() { return std::thread(&Listener::listener_function, this); }

void Listener::listener_function()
{
  LogFileOutput("Listener::listener_function - RUNNING\n");
  int server_fd, new_socket;
  struct sockaddr_in address;
  socklen_t address_length = sizeof(address);

#ifdef WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
  {
    LogFileOutput("Listener::listener_function - Socket creation failed\n");
    return;
  }

  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  address.sin_addr.s_addr = inet_addr(ip_address_.c_str());

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    LogFileOutput("Listener::listener_function - setsockopt failed\n");
    return;
  }

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
  {
    LogFileOutput("Listener::listener_function - bind failed\n");
    return;
  }

  if (listen(server_fd, 3) < 0)
  {
    LogFileOutput("Listener::listener_function - listen failed\n");
    return;
  }

  while (is_listening_)
  {
    fd_set sock_set;
    FD_ZERO(&sock_set);
    FD_SET(server_fd, &sock_set);

    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    const int activity = select(server_fd + 1, &sock_set, nullptr, nullptr, &timeout);

    if (activity == SOCKET_ERROR)
    {
      LogFileOutput("Listener::listener_function - select failed\n");
      is_listening_ = false;
      break;
    }

    if (activity == 0)
    {
      // timeout occurred, no client connection. Still need to check is_listening_
      continue;
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &address_length)) == INVALID_SOCKET)
    {
      LogFileOutput("Listener::listener_function - accept failed\n");
      is_listening_ = false;
      break;
    }

    create_connection(new_socket);
  }

  LogFileOutput("Listener::listener_function - listener closing down\n");

  CLOSE_SOCKET(server_fd);
#ifdef WIN32
  WSACleanup();
#endif
}

// Creates a Connection object, which is how SP device(s) will register itself with our listener.
void Listener::create_connection(unsigned int socket)
{
  // Create a connection, give it some time to settle, else exit without creating listener to connection
  const std::shared_ptr<Connection> conn = std::make_shared<TCPConnection>(socket);
  conn->create_read_channel();

  const auto start = std::chrono::steady_clock::now();
  // Give the connection a generous 10 seconds to work.
  constexpr auto timeout = std::chrono::seconds(10);

  while (!conn->is_connected())
  {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - start) > timeout)
    {
      LogFileOutput("Listener::create_connection() - Failed to establish "
                    "connection, timed out.\n");
      return;
    }
  }

  // We need to send an INIT to device 01 for this connection, then 02, ...
  // until we get an error back This will determine the number of devices
  // attached.

  bool still_scanning = true;
  uint8_t unit_id = 1;

  // send init requests to find all the devices on this connection, or we have too many devices.
  while (still_scanning && (unit_id + next_device_id_) < 255)
  {
    LogFileOutput("SmartPortOverSlip listener sending request for unit_id: %d\n", unit_id);
    InitRequest request(Requestor::next_request_number(), unit_id);
    const auto response = Requestor::send_request(request, conn.get());
    const auto init_response = dynamic_cast<InitResponse *>(response.get());
    still_scanning = init_response->get_status() == 0;
    if (still_scanning)
      unit_id++;
  }

  const auto start_id = next_device_id_;
  const auto end_id = static_cast<uint8_t>(start_id + unit_id - 1);
  next_device_id_ = end_id + 1;
  // track the connection and device ranges it reported. Further connections can add to the devices we can target.
  LogFileOutput("SmartPortOverSlip listener creating connection for start: %d, end: %d\n", start_id, end_id);
  insert_connection(start_id, end_id, conn);
}

void Listener::start()
{
  is_listening_ = true;
  listening_thread_ = std::thread(&Listener::listener_function, this);
}

void Listener::stop()
{
  LogFileOutput("Listener::stop()\n");
  if (is_listening_)
  {
    // Give our own thread time to stop while we stop the connections.
    is_listening_ = false;

    for (auto &pair : connection_map_)
    {
      const auto &connection = pair.second;
      connection->set_is_connected(false);
      connection->join();
    }
    LogFileOutput("Listener::stop() ... joining listener until it stops\n");
    listening_thread_.join();
  }
  LogFileOutput("Listener::stop() ... finished\n");
}

// Returns the lower bound of the device id, and connection.
std::pair<uint8_t, std::shared_ptr<Connection>> Listener::find_connection_with_device(const uint8_t device_id) const
{
  std::pair<uint8_t, std::shared_ptr<Connection>> result;
  for (const auto &kv : connection_map_)
  {
    if (device_id >= kv.first.first && device_id <= kv.first.second)
    {
      result = std::make_pair(kv.first.first, kv.second);
      break;
    }
  }
  return result;
}

std::vector<std::pair<uint8_t, Connection *>> Listener::get_all_connections() const
{
  std::vector<std::pair<uint8_t, Connection *>> connections;
  for (const auto &kv : connection_map_)
  {
    for (uint8_t id = kv.first.first; id <= kv.first.second; ++id)
    {
      connections.emplace_back(id, kv.second.get());
    }
  }
  return connections;
}
