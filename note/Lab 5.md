# Lab 5

在本实验中，需要完成介于网络层与数据链路层之间的ARP（地址解析协议），以便数据报可以在以太网等链路层进行传输。

## 实验代码及讲解

ARP的实际功能即为建立<协议，协议地址>到MAC地址的映射关系，ARP数据报的具体结构如下：

![lab5_figure1](C:\Users\xiurui\Desktop\计算机书单\CS144\lab5_figure1.png)

其中的条目包括：硬件和协议类型及其地址长度、ARP数据报的操作类型（请求或回复）以及发送方和接收方的协议地址和硬件地址。

### 链路层数据帧发送

当发送链路层数据帧时，其传输目标的<协议，协议地址>是已知的（在本实验中协议固定为IPV4）。为了使得数据帧可以在链路层中进行传输，需要利用ARP以将<协议，协议地址>转化为MAC以填充数据帧，用于发送数据帧的函数如下：

```c++
 32 void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
 33     // convert IP address of next hop to raw 32-bit representation (used in ARP header)
 34     const uint32_t next_hop_ip = next_hop.ipv4_numeric();
 35     EthernetFrame ef;
 36     if(_addr_mp.count(next_hop_ip)) {
 37         if(_addr_mp[next_hop_ip].second != ETHERNET_BROADCAST){
 38             ef.header().type = EthernetHeader::TYPE_IPv4;
 39             ef.header().src = _ethernet_address;
 40             ef.header().dst = _addr_mp[next_hop_ip].second;
 41             ef.payload() = dgram.serialize();
 42             _frames_out.push(ef);
 43         }
 44     }
 45     else {
 46         ef.header().type = EthernetHeader::TYPE_ARP;
 47         ef.header().src = _ethernet_address;
 48         ef.header().dst = ETHERNET_BROADCAST;
 49         ARPMessage am;
 50         am.opcode = ARPMessage::OPCODE_REQUEST;
 51         am.sender_ip_address = _ip_address.ipv4_numeric();
 52         am.sender_ethernet_address = _ethernet_address;
 53         am.target_ethernet_address = {0x00,0x00,0x00,0x00,0x00,0x00};
 54         am.target_ip_address = next_hop_ip;
 55         ef.payload() = am.serialize();
 56         _frames_out.push(ef);
 57         _addr_mp[next_hop_ip] = make_pair(5000,ETHERNET_BROADCAST);
 58         _data_mp[next_hop_ip] = dgram;
 59     }
 60 }
```

在这里，`dgram` 为网络层向下传递的数据报、`next_hop` 为数据帧所需传输的目的IP地址。在这里，我们需要检查本地地址解析模块中是否存储了有关 `next_hop` 的实际映射，本实验中使用了哈希表 `unordered_map` 进行相关映射的保存及查询：

```c++
 44     std::unordered_map<uint32_t, std::pair<size_t,EthernetAddress>> _addr_mp{};
```

 在这里，哈希表以IP地址为索引，以<映射的剩余寿命，MAC地址>为值。

**36 - 44行**：如哈希表中保存了有关该IP地址的映射，则检查该映射是否有效，即检查其对应的MAC地址是否为广播地址。如其为广播地址，则说明针对该IP地址已发送了ARP请求查询对应的MAC地址，但ARP回复尚未到达，因此直接返回（在5秒内不再次发送ARP请求）；如其为正常地址，则说明本地保存了与该IP地址映射的MAC地址，在这里填充数据帧的源、目的MAC地址，并将 `dgram` 数据报作为数据帧的负载，最后将该数据帧发送。

**45 - 58行**：如哈希表中未保存有关该IP地址的映射，则说明在5秒以内未发送与该IP地址相关的ARP请求，在这里构建ARP请求数据帧。在这里，我们需要填充ARP数据报的条目，并将其作为ARP请求数据帧的负载，其特点为数据帧的目的地址为MAC广播地址，且ARP数据报的目的MAC地址为全0（根据测试用例可得）。

最后，在 `_addr_mp` 中建立有关目的IP地址的条目（剩余寿命为5000ms），以防止在短时间内多次发送ARP请求。将所需发送的IP数据报 `dgram` 以目的IP地址为索引保存在 `_data_mp` 中，以便在接收到ARP回复时发送该数据报：

```c++
 46     std::unordered_map<uint32_t,InternetDatagram> _data_mp{};
```

### 链路层数据帧接收

当接收链路层数据帧时，调用 `NetworkInterface::recv_frame` 完成实际的接收工作：

```c++
 63 optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
 64     if(frame.header().type == EthernetHeader::TYPE_IPv4 && frame.header().dst == _ethernet_address){
 65         InternetDatagram ret;
 66         if(ret.parse(frame.payload()) == ParseResult::NoError){
 67             return ret;
 68         }   
 69     }
 70     else if(frame.header().type == EthernetHeader::TYPE_ARP){
 71         ARPMessage am;
 72         if(am.parse(frame.payload()) == ParseResult::NoError){
 73             _addr_mp[am.sender_ip_address] = make_pair(30000,am.sender_ethernet_address);
 74             if(_data_mp.count(am.sender_ip_address)){
 75                 EthernetFrame ef;
 76                 ef.header().type = EthernetHeader::TYPE_IPv4;
 77                 ef.header().src = _ethernet_address;
 78                 ef.header().dst = am.sender_ethernet_address;
 79                 ef.payload() = _data_mp[am.sender_ip_address].serialize();
 80                 _frames_out.push(ef);
 81             }
 82             if(am.opcode == ARPMessage::OPCODE_REQUEST && am.target_ip_address == _ip_address.ipv    4_numeric()){
 83                 EthernetFrame ef;
 84                 ef.header().type = EthernetHeader::TYPE_ARP;
 85                 ef.header().src = _ethernet_address;
 86                 ef.header().dst = am.sender_ethernet_address;
 87                 ARPMessage reply;
 88                 reply.opcode = ARPMessage::OPCODE_REPLY;
 89                 reply.sender_ip_address = am.target_ip_address;
 90                 reply.target_ip_address = am.sender_ip_address;
 91                 reply.sender_ethernet_address = _ethernet_address;
 92                 reply.target_ethernet_address = am.sender_ethernet_address;
 93                 ef.payload() = reply.serialize();
 94                 _frames_out.push(ef);
 95             }
 96         }
 97     }
 98     return {};
 99 }
```

在这里，需要检查数据帧内数据报的具体类型：

**64 - 69行**：如数据报的类型为IP数据报，且数据帧的目的地址与本地MAC地址对应，则试图解析该数据报并返回（如解析失败则返回空）；

**70 - 96行**：如数据包的类型为ARP数据报，且解析该数据报成功，则将数据报的发送方IP地址和MAC地址映射添加至本地地址解析模块。如 `_data_mp` 中存在与发送方IP地址相对应的数据报条目，则构建数据帧并将该数据报发送。如ARP数据报的类型为ARP请求，且其目的IP地址与本地IP地址对应，则构建ARP回复数据帧，并发送。

### 时间流逝

```c++
102 void NetworkInterface::tick(const size_t ms_since_last_tick) {
103     for(auto& [key, val] : _addr_mp){
104         size_t release_time = val.first;
105         if(release_time > ms_since_last_tick){
106             val.first -= ms_since_last_tick;
107         }
108         else{
109             _addr_mp.erase(key);
110         }
111     }
112 }
```

在 `tick` 中模拟了时间流逝，需要检查地址映射模块中条目的剩余寿命，并将条目进行驱逐或递减其剩余寿命。

## 实验结果

```
[100%] Testing Lab 5...
Test project /home/xiurui/sponge/bulid
    Start 31: t_webget
1/2 Test #31: t_webget .........................   Passed    2.55 sec
    Start 32: arp_network_interface
2/2 Test #32: arp_network_interface ............   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   2.57 sec
[100%] Built target check_lab5
```

