# Lab 4

本实验的任务为将之前所有实验的工作进行融合，构建用于TCP链接的基本构件`TCPConnect`链接器。

## 实验代码及讲解

链接器将 `TCPSender` 传输器和 `TCPReceiver` 接收器搭接起来，其基本构架如下：

![lab4_figure1](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/figure/lab4_figure1.png)

链接器通过调用 `segment_received` 从网络中接受TCP段，并将其内容解析并分发至传输器和接收器。并且，链接器根据传输器和接收器的状态参数，主动或被动地构建TCP段并发送至网络。虽然总体构架看起来很简单，但其实际所需满足的细节有许多。在这里，从链接器关闭、接收TCP段、传输TCP段、时间流逝四个链接器行为进行讲解。

### 链接器关闭

在这里，链接器具有两种关闭方式：和**unclean shutdown**。

- **unclean shutdown**：当链接器接收或发送一个带有 `RST` 的TCP段时，链接器进行**脏关闭**，其将接收器的输入数据流和输出数据流均置为error状态，并使得链接器的`active()`返回假；链接器在两个条件下发送一个 `RST` 段：

  1. 本地输出器的超时重传次数超出阈值 `TCPConfig::MAX_RETX_ATTEMPTS`；
  2. 本地输出器在`active()`为真时调用析构函数；

  在这里，将发送 `RST` 段的工作包装到`send_rst()`函数中：

  ```
   14 void TCPConnection::send_rst(){
   15     while(!_segments_out.empty())
   16         _segments_out.pop();
   17     _sender.send_empty_segment();
   18     TCPSegment seg = _sender.segments_out().front();
   19     _sender.segments_out().pop();
   20     seg.header().rst = true;
   21     _segments_out.push(seg);
   22     _receiver.stream_out().set_error();
   23     _sender.stream_in().set_error();
   24     _active = false;
   25 }
  
  ```

  在发送 `RST` 时，先将传输队列中所有的TCP段驱逐，然后利用 `TCPSender` 的 `send_empty_segment()` 方法创建空TCP段，并在设置其 `RST` 后将其传入传输队列。最后，将本地链接器的状态正确设置即可（在这里利用 `_active` 私有成员标识链接器的关闭状态）。

- **clean shutdown**：该种终结状态与脏终结的区别为，终结后仅有`active()`返回假，但输入数据流和输出数据流不为error状态。在这里，构成**clean shutdown**需要有四个条件：

  1.  本地接收器的输入流状态为EOF；
  2.  本地输出器的输出流状态为EOF；
  3.  本地输出器的输出TCP段被全部确认；
  4.  对端输出器的输出TCP段被全部确认；

  在这里前三个条件都比较容易判断，而第四个条件无法直接获得，需要使用一些近似状态进行模拟。首先，当本地链接器主动关闭时，即本地链接器首先发送 `FIN` 段，则在自收到对端最后一个TCP段开始计时， 如 `(10 × cfg.rt_timeout)` 毫秒后仍未接收到对端的任意TCP段，则认为对端输出的TCP段已经被完全确认，条件四满足；当本地连接器被被动关闭时，条件四一定完成，其判断条件为本地输入流比本地输出流更早达到EOF状态。如本地连接器被被动关闭，则将 `_linger_after_streams_finish` 私有成员置为假，示意本地链接器无需等待对端，当前三个条件均满足时即可关闭本地链接器。

  当以上四个条件在任意情况下被满足时，则将本地链接器关闭（ `active()` 返回假）。在这里，设置了  `try_clean_closed()` 函数对上述四个条件进行判断，如四个条件均被满足则将本地链接器关闭：

  ```
   26 void TCPConnection::try_clean_closed(){
   27     if(_receiver.stream_out().eof() && _sender.stream_in().eof() &&  _sender.bytes_in_flight() ==     0){
   28         if(_linger_after_streams_finish == false || _linger_time >= _cfg.rt_timeout * 10){
   29             _active = false;
   30         }
   31     }
   32 }
  ```

### 接收TCP段

`TCPConnect`链接器通过 `TCPConnection::segment_received` 进行TCP段的实际接收：

```
 53 void TCPConnection::segment_received(const TCPSegment &seg) {
 54     _linger_time = 0;
 55     if(seg.header().rst){
 56         _receiver.stream_out().set_error();
 57         _sender.stream_in().set_error();
 58         _active = false;
 59         return;
 60     }
```

**53 - 60行**：当接收到TCP段时，将 `_linger_time` 置为0，其代表了自上次接收到对端TCP段过去的毫秒数。如接收的TCP段带有`RST`，则进行脏关闭，将输入输出流置为error状态，并将 `_active` 置为假。

```
 61     _receiver.segment_received(seg);
 62     if(seg.header().ack){
 63         _sender.ack_received(seg.header().ackno,seg.header().win);
 64         if(_sender.next_seqno_absolute() != 0){
 65             _sender.fill_window();
 66             update_queue();
 67         }
 68     }
```

**61 - 68行**：将收到的TCP段送至本地接收器，如果TCP段的 `ACK` 被设置，则该TCP段带有对端的确认信息，将该确认信息送至本地传输器。值得注意的是，来自对端的确认消息可能通知了其接收窗口的更新，本地传输器需要在对端窗口更新时及时填充窗口，在这里调用`_sender.fill_window()` 使得本地传输器尝试填充窗口，并调用 `update_queue` ：

```
 33 void TCPConnection::update_queue(){
 34     while(!_sender.segments_out().empty()){
 35         TCPSegment seg = _sender.segments_out().front();
 36         if(_receiver.ackno().has_value()){
 37             seg.header().ack = true;
 38             seg.header().ackno = _receiver.ackno().value();
 39             seg.header().win = _receiver.window_size();
 40         }
 41         _segments_out.push(seg);
 42         _sender.segments_out().pop();
 43     }
 44 }
```

`update_queue` 将将本地传输器的传输队列中的TCP段移至本地链接器的传输队列，并在本地接收器开始工作时（`_receiver.ackno().has_value()`）为TCP段添加`ACK`、`ackno`和`win`信息。需要注意的是，仅在本地链接器已经和对端建立链接时（`SYN` 已发送，即`_sender.next_seqno_absolute() != 0`），才在接收 `ACK` 段时尝试填充窗口，否则会导致本地链接器错误地与对端建立链接。

```
 69     if(_receiver.stream_out().eof() && !_sender.stream_in().eof())
 70         _linger_after_streams_finish = false;
 71     try_clean_closed();
```

**69 - 71行**：当本次接收为对端发送的主动关闭时，需要将 `_linger_after_streams_finish` 置为假，表示本地链接器被被动关闭，无需等待对端。在这里，接收TCP端可能会导致本地链接器的状态变化，因此需要调用 `try_clean_closed()` 测试当前链接器是否满足关闭条件。

```
 72     if(seg.length_in_sequence_space() != 0 && _receiver.ackno().has_value()){
 73         if(_sender.next_seqno_absolute() == 0 && seg.header().syn == 1)
 74             _sender.fill_window();
 75         else
 76             _sender.send_empty_segment();
 77         update_queue();
 78     }
```

**72 - 78行**：当接收到的TCP段带有信息（即长度不为零），且本地接收器开始工作时，则需要向对端发送 `ACK` 信息。在这里，如果接收到的为 `SYN` 段，且本地链接器尚未建立链接，则通过调用 `_sender.fill_window()` 与对端建立连接（`_sender` 的首次填充窗口会导致一个 `SYN` 段被产生）；如已建立连接，则仅需调用 `_sender.send_empty_segment()` 生成一个空段。然后调用 `update_queue()` 完成实际的 `ACK` 信息填充和TCP段转移。

```
 79     if(seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value() && seg.header().seqno    == _receiver.ackno().value() - 1){
 80         _sender.send_empty_segment();
 81         update_queue();
 82     }
```

**79 - 82行**：最后，当对端发送了一个保活TCP段时（长度为0，序列号为本地接收器的确认号减一），且本地接收器开始工作时，则向对端发送 `ACK` 信息。

### 传输TCP段

在这里，链接器传输TCP段的行为可以分为主动传输和被动传输，其中被动传输在链接器接收TCP段或时间流逝时发生。在本段将介绍TCP段的主动传输，即主动建立链接、用户写入数据流和主动关闭：

```
112 void TCPConnection::connect() {
113     _sender.fill_window();
114     update_queue();
115 }
```

`TCPConnection::connect()` 执行了本地链接器的主动链接，其利用 `_sender.fill_window()` 生成一个`SYN`段，并将其移至链接器输出队列。

```
 87 size_t TCPConnection::write(const string &data) {
 88     size_t ret = _sender.stream_in().write(data);
 89     _sender.fill_window();
 90     update_queue();
 91     return ret;
 92 }
```

`TCPConnection::write` 执行了用户写入数据流。当用户写入数据时，本地传输器需要尝试填充窗口，并将填充的TCP段移至链接器输出队列。

```
106 void TCPConnection::end_input_stream() {
107     _sender.stream_in().end_input();
108     _sender.fill_window();
109     update_queue();
110 }
```

`TCPConnection::end_input_stream()` 执行了本地连接器的主动关闭，其将本地输出流的状态置为 `eof` ，然后调用`_sender.fill_window()` 生成一个`FIN`段，并将其移至链接器输出队列。

### 时间流逝

```
 95 void TCPConnection::tick(const size_t ms_since_last_tick) {
 96     _sender.tick(ms_since_last_tick);
 97     update_queue();
 98     _linger_time += ms_since_last_tick;
 99     if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
100         send_rst();
101         return;
102     }
103     try_clean_closed();
104 }
```

**96 - 98行**：当时间流逝时，需要将时间流逝信息通知本地传输器。值得注意的是，时间流逝可能导致新的TCP段被发送，需要调用 `update_queue()` 将新产生的TCP段转移。然后，递增 `_linger_time` 用于链接器的关闭信息。

**99 - 103行**：检查本地传输器的重传次数，如其大于阈值则执行脏关闭。最后，时间流逝可能导致本地链接器满足关闭条件，调用`try_clean_closed()` 测试其是否满足了关闭条件。

至此，有关链接器的所有行为均被介绍完毕。下面是一些用于导出链接器状态的简单接口：

```
 45 size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_    capacity(); }
 46 
 47 size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }
 48 
 49 size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }
 50 
 51 size_t TCPConnection::time_since_last_segment_received() const { return _linger_time; }
...
 85 bool TCPConnection::active() const { return _active; }
```

## 实验结果

```
157/162 Test #164: t_isnR_128K_8K_l .................   Passed    0.53 sec
        Start 165: t_isnR_128K_8K_L
158/162 Test #165: t_isnR_128K_8K_L .................   Passed    0.25 sec
        Start 166: t_isnR_128K_8K_lL
159/162 Test #166: t_isnR_128K_8K_lL ................   Passed    0.64 sec
        Start 167: t_isnD_128K_8K_l
160/162 Test #167: t_isnD_128K_8K_l .................   Passed    0.41 sec
        Start 168: t_isnD_128K_8K_L
161/162 Test #168: t_isnD_128K_8K_L .................   Passed    0.28 sec
        Start 169: t_isnD_128K_8K_lL
162/162 Test #169: t_isnD_128K_8K_lL ................   Passed    0.50 sec

100% tests passed, 0 tests failed out of 162

Total Test time (real) =  44.62 sec
[100%] Built target check_lab4
```

以上为 `make check_lab4` 的结果，测试了链接器的功能正确性。

```
CPU-limited throughput                : 0.38 Gbit/s
CPU-limited throughput with reordering: 0.27 Gbit/s
```

以上为`./app/tcp_benchmark` 的结果，测试了链接器的性能指标。

```
  1 #include "tcp_sponge_socket.hh"
  2 #include "util.hh"
  3 
  4 #include <cstdlib>
  5 #include <iostream>
  6 
  7 using namespace std;
  8 
  9 void get_URL(const string &host, const string &path) {
 10     // Your code here.
 11 
 12     // You will need to connect to the "http" service on
 13     // the computer whose name is in the "host" string,
 14     // then request the URL path given in the "path" string.
 15 
 16     // Then you'll need to print out everything the server sends back,
 17     // (not just one call to read() -- everything) until you reach
 18     // the "eof" (end of file).
 19     CS144TCPSocket sock;
 20     sock.connect(Address(host,"http"));
 21 
 22     string URL;
 23     URL += "GET " + path + " HTTP/1.1\r\n";
 24     URL += "Host: " + host + "\r\n";
 25     URL += "Connection: close\r\n\r\n";
 26     sock.write(URL);
 27     string readbuf, buf;
 28     while(!sock.eof()){
 29         sock.read(readbuf);
 30         buf += readbuf;
 31     }
 32     cout<<buf;
 33     sock.wait_until_closed();
 34 
 35 }
```

当将 `webget` 的TCP接口更改为实验实现的TCP接口时，工作正常：

```
Test project /home/xiurui/sponge/bulid
    Start 31: t_webget
1/1 Test #31: t_webget .........................   Passed    1.29 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   1.30 sec
[100%] Built target check_webget
```



