#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ef;
    if(_addr_mp.count(next_hop_ip)) {
        if(_addr_mp[next_hop_ip].second != ETHERNET_BROADCAST){
            ef.header().type = EthernetHeader::TYPE_IPv4;
            ef.header().src = _ethernet_address;
            ef.header().dst = _addr_mp[next_hop_ip].second;
            ef.payload() = dgram.serialize();
            _frames_out.push(ef);
        }
    }
    else {
        ef.header().type = EthernetHeader::TYPE_ARP;    
        ef.header().src = _ethernet_address;
        ef.header().dst = ETHERNET_BROADCAST;
        ARPMessage am;
        am.opcode = ARPMessage::OPCODE_REQUEST;
        am.sender_ip_address = _ip_address.ipv4_numeric();
        am.sender_ethernet_address = _ethernet_address;
        am.target_ethernet_address = {0x00,0x00,0x00,0x00,0x00,0x00};
        am.target_ip_address = next_hop_ip;
        ef.payload() = am.serialize();
        _frames_out.push(ef);
        _addr_mp[next_hop_ip] = make_pair(5000,ETHERNET_BROADCAST);
        _data_mp[next_hop_ip] = dgram;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().type == EthernetHeader::TYPE_IPv4 && frame.header().dst == _ethernet_address){
        InternetDatagram ret;
        if(ret.parse(frame.payload()) == ParseResult::NoError){
            return ret;
        }
    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage am; 
        if(am.parse(frame.payload()) == ParseResult::NoError){
            _addr_mp[am.sender_ip_address] = make_pair(30000,am.sender_ethernet_address);
            if(_data_mp.count(am.sender_ip_address)){
                EthernetFrame ef;
                ef.header().type = EthernetHeader::TYPE_IPv4;
                ef.header().src = _ethernet_address;
                ef.header().dst = am.sender_ethernet_address;
                ef.payload() = _data_mp[am.sender_ip_address].serialize();
                _frames_out.push(ef);
            }
            if(am.opcode == ARPMessage::OPCODE_REQUEST && am.target_ip_address == _ip_address.ipv4_numeric()){
                EthernetFrame ef;
                ef.header().type = EthernetHeader::TYPE_ARP;    
                ef.header().src = _ethernet_address;
                ef.header().dst = am.sender_ethernet_address;
                ARPMessage reply;
                reply.opcode = ARPMessage::OPCODE_REPLY;
                reply.sender_ip_address = am.target_ip_address;
                reply.target_ip_address = am.sender_ip_address;
                reply.sender_ethernet_address = _ethernet_address;
                reply.target_ethernet_address = am.sender_ethernet_address;
                ef.payload() = reply.serialize();
                _frames_out.push(ef);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for(auto& [key, val] : _addr_mp){
        size_t release_time = val.first;
        if(release_time > ms_since_last_tick){
            val.first -= ms_since_last_tick;
        }
        else{
            _addr_mp.erase(key);
        }
    }
}
