
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#pragma once

struct FirewallFormatException : public std::exception {
  const char *what() const throw() { return "Firewall file format error!"; }
};

class Firewall {
 public:
  Firewall(std::string filename) : filename_(filename) {}

  bool permit(std::string cmd, std::string ip) {
    if (cmd != "CONNECT" && cmd != "BIND") {
      std::cout << "Error: cmd should be CONNECT or BIND." << std::endl;
      return false;
    }
    std::string req_type = (cmd == "CONNECT") ? "c" : "b";
    std::regex ip_regex("(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)");
    std::regex firewall_regex(
        "(\\d+|\\*)\\.(\\d+|\\*)\\.(\\d+|\\*)\\.(\\d+|\\*)");
    std::smatch match;
    if (!std::regex_match(ip, match, ip_regex)) {
      std::cout << "Error: incoming ip format error" << std::endl;
      return false;
    }

    std::string line;
    std::ifstream file(filename_);
    while (std::getline(file, line)) {
      std::string r_name, r_type, r_ip;
      std::stringstream ss(line);
      ss >> r_name >> r_type >> r_ip;
      std::smatch r_match;
      if (r_name != "permit" || !(r_type == "b" || r_type == "c") ||
          !std::regex_match(r_ip, r_match, firewall_regex)) {
        throw FirewallFormatException();
      }
      if (r_type != req_type) {
        continue;
      }
      bool pass = true;
      for (int i = 1; i < 5; i++) {
        if (r_match[i].str() != "*" && r_match[i].str() != match[i].str()) {
          pass = false;
          break;
        }
      }
      if (pass) {
        return true;
      }
    }
    return false;
  }

 private:
  std::string filename_;
};
