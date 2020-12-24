#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <string>

#include "base_handler.cpp"
#include "socks_packet_parser.cpp"
#include "utils.cpp"

#define MAX_LENGTH 1024

using boost::asio::ip::tcp;

class ConnectHandler : public BaseHandler<ConnectHandler> {
 public:
  ConnectHandler(tcp::socket socket, boost::asio::io_context &io_context,
                 Socks4RequestPacket &request, std::string conf_file)
      : BaseHandler(std::move(socket), io_context, request, conf_file) {}

  void Start() {
    auto self(shared_from_this());
    HandleStart(boost::bind(&ConnectHandler::HandleResolve, self, //unique
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::results));
  }

 private:
  void HandleConnect(const boost::system::error_code &err) {
    if (err) {
      std::cout << "Error(HandleConnect): " << err.message() << std::endl;
      HandleReply(false, "CONNECT", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      return;
    }
    HandleReply(true, "CONNECT", client_socket_.remote_endpoint(),
                dst_endpoint_);
    StartRelayTraffic();
  }

  void HandleResolve(const boost::system::error_code &err,
                     tcp::resolver::results_type results) {
    if (err) {
      std::cout << "Error(HandleResolve): " << err.message() << std::endl;
      HandleReply(false, "CONNECT", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      return;
    }
    dst_endpoint_ = (*results).endpoint();// resolve 完的第一個 IP(可能domain name 對到多個ip)
    if (!firewall_.permit("CONNECT", dst_endpoint_.address().to_string())) {
      HandleReply(false, "CONNECT", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      std::cout << "Block CONNECT: " << dst_endpoint_ << std::endl;
      return;
    }
    auto self(shared_from_this());
    server_socket_.async_connect( //直接繼承 base 的變數，這邊弄好 server
        dst_endpoint_, boost::bind(&ConnectHandler::HandleConnect, self,
                                   boost::asio::placeholders::error));
  }
};
