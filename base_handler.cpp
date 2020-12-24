#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include <iostream>
#include <string>

#include "firewall.cpp"
#include "socks_packet_parser.cpp"
#include "utils.cpp"

#pragma once
#define MAX_LENGTH 1024

using boost::asio::ip::tcp;

template <class T>
class BaseHandler : public std::enable_shared_from_this<T> { //繼承者也可以使用 share 的功能 template 可以接收傳進來的不同類型資料
 public:
  BaseHandler(tcp::socket socket, boost::asio::io_context &io_context,
              Socks4RequestPacket &request, std::string conf_file) // initalize 完變數就消失了
      : dst_endpoint_(boost::asio::ip::address::from_string(request.dst_ip), // 冒號後面為初始化
                      request_.dst_port),
        client_socket_(std::move(socket)),
        server_socket_(io_context),
        resolver_(io_context),
        request_(request), // 複製一份值
        firewall_(conf_file) {}

  void Start() {}

 protected:
  void SendSocks4Reply(std::string reply_str) {
    char *reply = new char[reply_str.length()];
    memcpy(reply, reply_str.c_str(), reply_str.length());
    // Write reply packet
    auto self(this->shared_from_this());
    boost::asio::async_write(
        client_socket_, boost::asio::buffer(reply, reply_str.length()),
        [this, self, reply](const boost::system::error_code &err,
                            std::size_t bytes_transferred) {
          if (err) {
            std::cout << "Error(async_write): " << err.message() << std::endl;
            return;
          }
        });
  }

  void HandleReply(bool succ, std::string type, tcp::endpoint c_endpoint,
                   tcp::endpoint s_endpoint) {
    std::string result = succ ? "Accept" : "Reject";
    int code = succ ? 90 : 91;
    Log(c_endpoint.address().to_string(), c_endpoint.port(),
        s_endpoint.address().to_string(), s_endpoint.port(), type, result);
    SendSocks4Reply(BuildReplyPacket(0, code, 0, "0.0.0.0"));
  }

  void HandleStart(boost::function<void(boost::system::error_code,
                                        tcp::resolver::results_type)>
                       resolve_handler) {//function
    std::string host = 
        request_.do_resolve ? request_.domain_name : request_.dst_ip;
    resolver_.async_resolve(host, std::to_string(request_.dst_port),
                            resolve_handler);
  }

  void HandleWrite(const boost::system::error_code &err,
                   std::size_t bytes_transferred, tcp::socket &r_socket,
                   tcp::socket &w_socket, char *data) {
    if (err) {
      std::cout << "Error(HandleWrite): " << err.message() << std::endl;
      return;
    }
    Relay(r_socket, w_socket, data);
  }

  void HandleRead(const boost::system::error_code &err,
                  std::size_t bytes_transferred, tcp::socket &r_socket,
                  tcp::socket &w_socket, char *data) {
    if (err) {
      if (err != boost::asio::error::eof &&
          err != boost::asio::error::operation_aborted) {
        std::cout << "Error(HandleRead): " << err.message() << std::endl;
        return;
      }
      client_socket_.close(); // connect 會一直互傳 某方服務結束才會 eof
      server_socket_.close(); // 每個 bind request 都只有一個方向
      return;
    }
    auto self(this->shared_from_this());
    boost::asio::async_write(
        w_socket, boost::asio::buffer(data, bytes_transferred),
        boost::bind(&BaseHandler::HandleWrite, self,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    boost::ref(r_socket), boost::ref(w_socket), data));
  }

  void Relay(tcp::socket &r_socket, tcp::socket &w_socket, char *data) {
    auto self(this->shared_from_this());
    r_socket.async_read_some(
        boost::asio::buffer(data, kMaxLength),
        boost::bind(&BaseHandler::HandleRead, self,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    boost::ref(r_socket), boost::ref(w_socket), data));
  }

  void StartRelayTraffic() {
    // Read from client and write to server
    Relay(client_socket_, server_socket_, data1_);
    // Read from server and write to client
    Relay(server_socket_, client_socket_, data2_);
  }

  tcp::endpoint dst_endpoint_;
  tcp::socket client_socket_;
  tcp::socket server_socket_;
  tcp::resolver resolver_;
  Socks4RequestPacket request_;
  Firewall firewall_;
  enum { kMaxLength = 1024 };
  char data1_[kMaxLength];
  char data2_[kMaxLength];
};
