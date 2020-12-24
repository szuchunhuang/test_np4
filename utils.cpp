#include <iostream>
#include <string>

#pragma once

void Log(std::string s_ip, unsigned short s_port, std::string d_ip,
         unsigned short d_port, std::string cmd, std::string reply) {
  std::cout << "-----------" << std::endl;
  std::cout << "<S_IP>: " << s_ip << std::endl;
  std::cout << "<S_PORT>: " << s_port << std::endl;
  std::cout << "<D_IP>: " << d_ip << std::endl;
  std::cout << "<D_PORT>: " << d_port << std::endl;
  std::cout << "<Command>: " << cmd << std::endl;
  std::cout << "<Reply>: " << reply << std::endl;
}

std::string BuildReplyPacket(int version, int cmd, unsigned short port,
                             std::string ip) {
  std::string packet;
  packet += version;
  packet += cmd;
  packet += port / 256;
  packet += port % 256;
  for (int i = 0; i < 4; i++) {
    int idx = ip.find(".");
    std::string token = ip.substr(0, idx);
    packet += std::stoi(token);
    ip = ip.substr(idx + 1);
  }
  return packet;
}
