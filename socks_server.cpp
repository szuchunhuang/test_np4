#include <sys/wait.h>

#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "bind_handler.cpp"
#include "connect_handler.cpp"
#include "firewall.cpp"
#include "socks_packet_parser.cpp"
#include "utils.cpp"

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
 public:
  session(tcp::socket socket, boost::asio::io_context &io_context)
      : socket_(std::move(socket)), io_context(io_context), firewall_client(conf_file_client) {}

  void start() { do_read(); }

 private:
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code err, std::size_t length) {
          if (err) {
            if (err != boost::asio::error::eof) {
              std::cout << "Error(async_read_some): " << err.message()
                        << std::endl;
            }
            return;
          }
          // std::cout << "-----------" << std::endl;
          // std::cout << "Read raw: " << data_ << std::endl;
          // std::cout << "Read: ";
          // for (int i = 0; i < length; i++)
          // {
          //   std::cout << (int)data_[i] << " ";
          // }
          // std::cout << std::endl;
          Socks4RequestPacket request;
          try {
            request = ParseSocks4Request(data_, length);
          } catch (RequestFormatException &e) {
            std::cout << e.what() << std::endl;
            return;
          }
          
          if (!firewall_client.permit("CONNECT", socket_.remote_endpoint().address().to_string())) {
            std::cout << "Block client: " << socket_.remote_endpoint().address().to_string() << std::endl;
            return;
          }

          if (request.cmd == 1) {
            // Connect Request
            std::make_shared<ConnectHandler>(std::move(socket_), io_context,
                                             request, conf_file)
                ->Start();
          } else {
            // Bind Request
            std::make_shared<BindHandler>(std::move(socket_), io_context,
                                          request, conf_file)
                ->Start();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::io_context &io_context;
  std::string conf_file = "socks.conf";
  std::string conf_file_client = "client_socks.conf";
  Firewall firewall_client;
  enum { max_length = 1024 };
  unsigned char data_[max_length];
};

class server {
 public:
  server(boost::asio::io_context &io_context, short port)
      : signal_(io_context, SIGCHLD),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    wait_for_signal();
    do_accept(io_context);
  }

 private:
  void wait_for_signal() {
    signal_.async_wait([this](boost::system::error_code /*ec*/, int /*signo*/) {
      // Only parents need to check for signal
      if (acceptor_.is_open()) {
        // Wait completed child
        int status = 0;
        while (waitpid(-1, &status, WNOHANG) > 0) {
        }
        wait_for_signal();
      }
    });
  }
  void do_accept(boost::asio::io_context &io_context) {
    acceptor_.async_accept(
        [this, &io_context](boost::system::error_code err, tcp::socket socket) {
          if (err) {
            std::cout << "Error(async_accept): " << err.message() << std::endl;
            do_accept(io_context);
            return;
          }
          io_context.notify_fork(boost::asio::io_context::fork_prepare);
          if (fork() == 0) {
            io_context.notify_fork(boost::asio::io_context::fork_child);
            // Close acceptor for child
            acceptor_.close();
            signal_.cancel();
            std::make_shared<session>(std::move(socket), io_context)->start();
          } else {
            io_context.notify_fork(boost::asio::io_context::fork_parent);
            // Close newly accepted socket for parent
            socket.close();
            do_accept(io_context);
          }
        });
  }

  boost::asio::signal_set signal_;
  tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}