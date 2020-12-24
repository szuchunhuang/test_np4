#include <array>
#include <map>
#include <string>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio.hpp>
// #include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>
#include <unistd.h>
#include <boost/bind/bind.hpp>
#include "utils.cpp"

// #define DEBUG 1
using boost::asio::ip::tcp;
using namespace std;
using namespace boost::asio;
using namespace boost::asio::placeholders;

vector<array<string, 3>> Host_Port_FilePath;
vector<array<string, 2>> SockHost_SockPort;
vector<string> cut_query_string;
vector<string> tmp;
io_context my_io_context;
bool sock_connect = false;

void Parse_QUERY_STRING() {
    string raw_query_str(getenv("QUERY_STRING"));
    boost::algorithm::split(cut_query_string, raw_query_str, boost::algorithm::is_any_of("&"));
    for (int i = 0; i < 15; i = i + 3) {
        if (cut_query_string[i].size() > 3) {
            string host = cut_query_string[i].substr(3, string::npos);
            string port = cut_query_string[i+1].substr(3, string::npos);
            string filePath = cut_query_string[i+2].substr(3, string::npos);
            Host_Port_FilePath.push_back(array<string, 3>({host, port, filePath}));
        }
    }
    string sock_host = cut_query_string[15].substr(3, string::npos);
    string sock_port = cut_query_string[16].substr(3, string::npos);
    if (sock_host != "") {
        sock_connect = true;
        SockHost_SockPort.push_back(array<string, 2>({sock_host, sock_port}));
    }

}

void Initial_Html_Output() {
    cout << "Content-type: text/html\r\n\r\n";
    cout << R"html(<!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8" />
        <title>NP Project 3 Sample Console</title>
        <link
        rel="stylesheet"
        href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
        integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
        crossorigin="anonymous"
        />
        <link
        href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
        rel="stylesheet"
        />
        <link
        rel="icon"
        type="image/png"
        href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
        />
        <style>
        * {
            font-family: 'Source Code Pro', monospace;
            font-size: 1rem !important;
        }
        body {
            background-color: #212529;
        }
        pre {
            color: #cccccc;
        }
        b {
            color: #01b468;
        }
        </style>
    </head>
    <body>
        <table class="table table-dark table-bordered">
        <thead>
            <tr>)html"
    << endl;
    for (unsigned int i = 0; i < Host_Port_FilePath.size(); i++) {
        cout << "          <th scope=\"col\">" << Host_Port_FilePath[i][0] << ":" << Host_Port_FilePath[i][1] << "</th>" << endl;
    }      
    cout << R"html(
            </tr>
        </thead>
        <tbody>
            <tr>)html" 
    << endl;
    for (unsigned int i = 0; i < Host_Port_FilePath.size(); i++) {
        cout << "          <td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>" << endl;
    }
    cout << R"html(
            </tr>
        </tbody>
        </table>
    </body>
    </html>)html"
    << endl;
}

void Html_Output(int Session_ID, string line, bool command_from_file) { // 1 是印出 txt 指令, 0 是印出 golden 回傳的指令
    boost::algorithm::replace_all(line, "\'", "&#39;");
    boost::algorithm::replace_all(line, "\n", "&NewLine;");
    boost::algorithm::replace_all(line, "\r", "");
    boost::algorithm::replace_all(line, "<", "&#60;");
    boost::algorithm::replace_all(line, ">", "&#62;");

    if (command_from_file){
        line = "<b>" + line + "</b>";
    }
    cout << "<script>document.getElementById('s" << Session_ID << "').innerHTML += '" << line << "';</script>" << flush;
} 


// void MultiEndpoints_Connect() {
//     ip::tcp::resolver resolver(my_io_context);
//     ip::tcp::resolver::iterator iter;
    
//     for(unsigned int i = 0; i < Host_Port_FilePath.size(); i++){
//         ip::tcp::resolver::query query(Host_Port_FilePath[i][0], Host_Port_FilePath[i][1]);
//         iter = resolver.resolve(query);
//         shared_ptr<session> sess_pointer = make_shared<session>(i, Host_Port_FilePath[i][2]);
//         sess_pointer->start(iter);
//     }
// }


class session : public std::enable_shared_from_this<session> {
// public:
private:
    int Session_ID;
    tcp::socket socket_;
    ifstream CommandFile;
    tcp::resolver resolver_;
    string golden_host_, golden_port_;

public:
    session(int id, string FilePath, string golden_host, string golden_port)
        : Session_ID(id), socket_(my_io_context), CommandFile("./test_case/" + FilePath), resolver_(my_io_context), 
        golden_host_(golden_host), golden_port_(golden_port) {}

    void start(ip::tcp::resolver::iterator iter) {
        auto self(shared_from_this());
        socket_.async_connect(
            iter->endpoint(),
            [this, self](boost::system::error_code ec) {
                if (sock_connect) {
                    sock_connection();
                } else {
                    do_read();
                }
            });
    }

private:
    enum { max_length = 1024 };
    char data_[max_length];

    void HandleResolve(const boost::system::error_code ec, tcp::resolver::results_type results) {
        if (ec) {
            std::cout << "Error(console_cgi_HandleResolve): " << ec << std::endl;
            return;
        }
        auto self(shared_from_this());
        int user_id = 0; // USERID is NULL
        std::string dst_ip;
        std::string sock_request;
        tcp::endpoint s_endpoint = (*results).endpoint();
        dst_ip = s_endpoint.address().to_string();
        sock_request = BuildReplyPacket(4, 1, s_endpoint.port(), dst_ip);
        sock_request += user_id; 
        socket_.async_send(boost::asio::buffer(sock_request, sock_request.size()),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    socket_.async_read_some(buffer(data_, 8),
                        [this, self](boost::system::error_code ec, size_t length) {
                            if (ec) {
                                std::cout << "Error(console_cgi_async_read_some): " << ec << std::endl;
                            }
                            if (data_[1] != 90) {
                                std::cout << "sock_server reject connection!" << std::endl;
                                return;
                            } else {
                                do_read();
                            }
                            
                        });
                }
            });
       
    }

    void sock_connection() {
        auto self(shared_from_this());
        resolver_.async_resolve(
            golden_host_, golden_port_,
            boost::bind(
                &session::HandleResolve, self,
                boost::asio::placeholders::error,
                boost::asio::placeholders::results
            ));
    }


    void do_read() { // 從 golden 端寫入
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    string output_line(data_, length);
                    Html_Output(Session_ID, output_line, 0);
                    if (output_line.find("% ") != string::npos) {
                        do_write();
                    }
                    do_read();
                }
            });
    }

    void do_write() { // 寫指令到 golden 端
        auto self(shared_from_this());
        string command_line;
        if (getline(CommandFile, command_line)) {
            command_line += "\n";
            socket_.async_send(boost::asio::buffer(command_line, command_line.size()),
                [this, self, command_line](boost::system::error_code ec, size_t length) {
                    if (!ec) {
                        Html_Output(Session_ID, command_line, 1);
                    }
                }
            );
        }
    }
};

int main(int argc, char* argv[])
{
  try
  {
    Parse_QUERY_STRING();
    Initial_Html_Output();
    // MultiEndpoints_Connect(); 
    ip::tcp::resolver resolver(my_io_context);
    ip::tcp::resolver::iterator iter;
    
    for(unsigned int i = 0; i < Host_Port_FilePath.size(); i++){
        if (sock_connect) {
            ip::tcp::resolver::query query(SockHost_SockPort[0][0], SockHost_SockPort[0][1]);
            iter = resolver.resolve(query);
        } else {
            ip::tcp::resolver::query query(Host_Port_FilePath[i][0], Host_Port_FilePath[i][1]);
            iter = resolver.resolve(query);
        }
        shared_ptr<session> sess_pointer = make_shared<session>(i, Host_Port_FilePath[i][2], Host_Port_FilePath[i][0], Host_Port_FilePath[i][1]);
        sess_pointer->start(iter);
    }

    my_io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}