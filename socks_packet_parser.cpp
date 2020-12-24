#include <iostream>
#include <string>
#include <cstring>

#pragma once

#define MAX_LENGTH 1024


struct RequestFormatException : public std::exception 
{
   const char *what () const throw(){
       return "Packet format error!";
   }
};

struct Socks4RequestPacket
{
    // Get the destination IP and port from SOCKS4 REQUEST
    // SOCKS4_REQUEST packet (VN=4, CD=1 or 2) Type 1: CONNECT
    //       +----+----+----+----+----+----+----+----+----+----+....+----+
    //       | VN | CD | DSTPORT | DSTIP        | USERID            |NULL|
    //       +----+----+----+----+----+----+----+----+----+----+....+----+
    // bytes:   1    1      2              4              variable     1

    int version;
    int cmd;
    unsigned short dst_port;
    std::string dst_ip;
    std::string userid;
    std::string domain_name;
    bool do_resolve = false;
};

Socks4RequestPacket ParseSocks4Request(unsigned char data[MAX_LENGTH], std::size_t length)
{
    Socks4RequestPacket packet;

    if (length <= 8 || data[0] != 4)
    {
        throw RequestFormatException();
    }

    packet.version = data[0];
    packet.cmd = data[1];
    packet.dst_port = (unsigned short)data[2] * 256 + (unsigned short)data[3];
    packet.dst_ip = std::to_string(data[4]) + "." + std::to_string(data[5]) + "." + std::to_string(data[6]) + "." + std::to_string(data[7]);

    std::size_t i = 8;
    while (i < length && data[i] != '\0')
    {
        packet.userid.append(std::to_string((unsigned int)data[i]));
        i++;
    }
    if (data[4] == 0 && data[5] == 0 && data[6] == 0 && data[7] != 0)
    {
        packet.do_resolve = true;
        i++;
        while (i < length && data[i] != '\0')
        {
            packet.domain_name += data[i];
            i++;
        }
    }
    return packet;
}
