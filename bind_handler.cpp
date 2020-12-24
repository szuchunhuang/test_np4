#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <string>

#include "base_handler.cpp"
#include "socks_packet_parser.cpp"
#include "utils.cpp"

#define MAX_LENGTH 1024

using boost::asio::ip::tcp;

class BindHandler : public BaseHandler<BindHandler> {
 public:
  BindHandler(tcp::socket socket, boost::asio::io_context &io_context,
              Socks4RequestPacket &request, std::string conf_file)
      : BaseHandler(std::move(socket), io_context, request, conf_file), // 宣告兩個屬於 bind 的變數
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 0)) {} // 任意指配新 port listen

  void Start() {
    auto self(shared_from_this());
    HandleStart(boost::bind(&BindHandler::HandleResolve, self, //unique
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::results));
  }

 private:
  void HandleAccept(const boost::system::error_code &err) {
    if (err) {
      std::cout << "Error(HandleAccept): " << err.message() << std::endl;
      HandleReply(false, "BIND", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      return;
    }
    // Check server ip match request
    // if (server_socket_.remote_endpoint().address().to_string() !=
    // request_.dst_ip)
    // {
    //   std::cout << "Error: IP mismatch!" << std::endl;
    //   std::cout << "Error: incoming connection IP: " <<
    //   server_socket_.remote_endpoint().address().to_string()  << std::endl;
    //   std::cout << "Error: request DSTIP: " << request_.dst_ip << std::endl;
    //   ReplySocks4(BuildReplyPacket(0, 91, 0, "0.0.0.0"));
    //   return;
    // }
    HandleReply(true, "BIND", client_socket_.remote_endpoint(), dst_endpoint_); //成功接到 bind 好
    StartRelayTraffic();
  }

  void HandleResolve(const boost::system::error_code &err,
                     tcp::resolver::results_type results) { //回傳好多port
    if (err) {
      std::cout << "Error(HandleResolve): " << err.message() << std::endl;
      HandleReply(false, "BIND", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      return;
    }
    dst_endpoint_ = (*results).endpoint(); //只拿第一個endpoint 因為同的domain name 可能有很多個endpoint(多ip連到它)
    if (!firewall_.permit("BIND", dst_endpoint_.address().to_string())) {
      std::cout << "Block BIND: " << dst_endpoint_ << std::endl;
      HandleReply(false, "BIND", client_socket_.remote_endpoint(),
                  dst_endpoint_);
      return;
    }
    SendSocks4Reply(
        BuildReplyPacket(0, 90, acceptor_.local_endpoint().port(), "0.0.0.0"));
    auto self(shared_from_this());
    acceptor_.async_accept(server_socket_, //傳進去代表他接收了傳進來的 server 代表執行完就連到了
                           boost::bind(&BindHandler::HandleAccept, self,
                                       boost::asio::placeholders::error));
  }

  tcp::acceptor acceptor_;
};
