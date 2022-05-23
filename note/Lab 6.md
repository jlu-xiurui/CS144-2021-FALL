# Lab 6

在本实验中，需要实现网络层中的IP路由器功能，其实现依赖于上实验中网络接口的实现。

## 实验源码及实现

路由器在工作中维护了一张路由表，其根据传入其中的数据报目的IP地址，从路由表中**匹配**得到对应的表项，并根据匹配结果执行相应的**行为**。在这里对具体的**匹配**和**行为**策略进行介绍：

1. 匹配：路由表由一系列**具有一定有效长度的前缀IP地址**作为其索引，当前缀IP地址作为数据报目的IP地址的前缀时（如 `18.47.x.y` 为 `18.47.255.255` 的前缀），该该表项为匹配结果的**候选表项**。在这里我们需要将**有效长度最长**的候选表项作为匹配结果返回（可能无候选表项）。
2. 行为：当匹配结束时，路由器需要根据匹配结果对传入其中的数据报进行一定的操作。如未匹配到相应的表项，则直接将该数据报抛弃；如匹配到表项，则根据表项中的 `interface_mum` 将数据报发送至相应的网络接口，并根据表项中的 `next_hop` 为网络接口指出该数据报的下一跳IP地址。值得注意的是，当 `next_hop` 为空时，需要将其替换为数据报的目的IP地址（该路由器与目的IP地址直接相连，而不经过其他路由器）。

```c++
 25 void Router::add_route(const uint32_t route_prefix,
 26                        const uint8_t prefix_length,
 27                        const optional<Address> next_hop,
 28                        const size_t interface_num) {
 29     // Your code here.
 30     uint32_t real_prefix = route_prefix >> (32 - prefix_length) << (32 - prefix_length);
 31     route_mps[prefix_length][real_prefix] = make_pair(next_hop,interface_num);
 32 }
```

`Router::add_route` 为路由表的表项插入函数，其 `route_prefix` 为前缀IP地址、`prefix_length` 为前缀IP地址的有效长度、`next_hop`为表项中存储的下一跳IP地址（可以为空）、`interface_num` 为表项中存储的网络接口编号。在本实验中，唯一的难点即为如何优雅的实现目的IP地址的匹配操作，在这里利用一个长度为32+1的哈希表数组解决该问题，其作为 `Route` 类的私有成员：

```c++
 45 class Router {
 46     //! The router's collection of network interfaces
 47     std::vector<AsyncNetworkInterface> _interfaces{};
 48 
 49     //! Send a single datagram from the appropriate outbound interface to the next hop,
 50     //! as specified by the route with the longest prefix_length that matches the
 51     //! datagram's destination address.
 52     void route_one_datagram(InternetDatagram &dgram);
 53 
 54     std::vector<std::unordered_map<uint32_t,std::pair<std::optional<Address>,size_t>>> route_mps;
```

在本函数中，将有效长度作为数组索引、前缀IP地址作为对应数组中的哈希表索引，以`next_hop`和`interface_num`作为值将路由表项存入路由器，具体的匹配方法见下文。

```c++
 35 void Router::route_one_datagram(InternetDatagram &dgram) {
 36     // Your code here.
 37     if(dgram.header().ttl == 0 || --dgram.header().ttl == 0)
 38         return;
 39     uint32_t dst = dgram.header().dst;
 40     for(int len = 32; len >= 0; len--){
 41         dst = len == 0 ? 0 : dst >> (32 - len) << (32 - len);
 42         if(route_mps[len].count(dst)){
 43             auto [next_hop,interface_num] = route_mps[len][dst];
 44             Address real_next_hop = next_hop.has_value() ? next_hop.value() : Address::from_ipv4_    numeric(dgram.header().dst);
 45             interface(interface_num).send_datagram(dgram,real_next_hop);
 46             return;
 47         }
 48     }
 49 
 50 }
```

`Router::route_one_datagram` 执行具体的路由表匹配及对应行为工作。

**36 - 38行**：在这里需要检查数据报的剩余跳数（TTL）并将其递减，如其为零或递减后为零则抛弃该数据报；

**40 - 42行**：执行实际的匹配工作，在这里利用了一种贪心的策略，即根据有效长度从大向小查找对应的哈希表，这样第一个查找到的候选表项即为有效长度最长的路由表项，且匹配的时间复杂度为O(33*C)常数级，其中33为数组长度、C为哈希表搜索时间复杂度。在这里，当遍历中的当前有效长度为`len`时，传入哈希表的键即为目的IP地址的前 `len` 位，哈希表的搜索结果即为对应的前缀匹配结果。

**43 - 46行**：当匹配成功时，我们根据表项中的值调用`interface(interface_num).send_datagram(dgram,real_next_hop)`执行实际的数据报转发工作，当 `next_hop` 为空时将其替换为数据报的目的IP地址。

## 实验结果

```
[100%] Testing Lab 6...
Test project /home/xiurui/sponge/bulid
    Start 32: arp_network_interface
1/2 Test #32: arp_network_interface ............   Passed    0.00 sec
    Start 33: router_test
2/2 Test #33: router_test ......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.01 sec
[100%] Built target check_lab6
```

