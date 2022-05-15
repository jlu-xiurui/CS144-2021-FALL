# Lab 3

在本实验中，需要完成 `TCPSender` 传输器，其将从应用程序输入的 `Outbound` 数据流转化为TCP段，并发送至互联网。

## 实验代码及讲解

### TCPSender传输器

 `TCPSender` 传输器的主要任务为：

1. 根据对端TCP接收器的窗口大小，尽可能地将本地数据流在转化为TCP段，并发送至对端；
2. 跟踪传输至对端且并未被对端确认的所有TCP段（称其为`Outstanding segment`），如果持续一定的时间内未有任何TCP段被对端确认，则将最早发送至对端的TCP段重新传输，并跟踪重传次数；
3. 当接收器接收到对端的 `ACK` 信息时，将所有字节完全被对端确认的 `Outstanding segment` 解除跟踪。如存在被完全确认的 `Outstanding segment` ，则刷新重传时间及重传次数。 并且，更新本地的输出窗口大小。

```
 48 class TCPSender {
 49   private:
 50     //! our initial sequence number, the number for our SYN.
 51     WrappingInt32 _isn;
 52 
 53     //! outbound queue of segments that the TCPSender wants sent
 54     std::queue<TCPSegment> _segments_out{};
 55 
 56     //! retransmission timer for the connection
 57     unsigned int _initial_retransmission_timeout;
 58 
 59     //! outgoing stream of bytes that have not yet been sent
 60     ByteStream _stream;
 61 
 62     //! the (absolute) sequence number for the next byte to be sent
 63     uint64_t _next_seqno{0};
 64 
 65     uint64_t _bytes_in_flight{0};
 66 
 67     uint16_t _window_size{1};
 68 
 69     uint64_t _ackno{0};
 70 
 71     bool _fin_sended{false};
 72 
 73     Timer _timer;
 74   public:
 75     //! Initialize a TCPSender
 76     TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
 77               const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
 78               const std::optional<WrappingInt32> fixed_isn = {});
```

其私有成员如上所示，`_isn` 记录了段序列号的基础值、`_segments_out`  用于将段传输至互联网，当TCP段被存放其中时，即视为其被发送至对端、`_initial_retransmission_timeout` 为超时重传阈值的初始值、`_stream` 为`Inbound` 数据流，用户通过该数据流将数据传入 `TCPSender` 、`_next_seqno` 记录了下一个需要被发送的字节的绝对序列号、`_bytes_in_flight` 记录了当前已被发送但未被确认的字节数、`_window_size` 记录了对端TCP接收端的窗口大小、`_ackno` 记录了对端TCP接收端所希望接受的第一个字节的绝对序列号、`_fin_sended` 用于记录 `FIN` 段是否被发送、`_timer` 控制了超时重传有关的所有任务。

### 超时重传

超时重传的具体策略具有许多细节：

1.  `TCPSender` 传输器通过调用 `tick` 函数告知内部时间流逝的具体毫秒数；

2. 在构造 `TCPSender` 时，可以定义超时重传的剩余时间（RTO）的初始值 `_initial retransmission timeout` 。当时间流逝时RTO会随之变化，但其初始值不会改变。

3. 当长度非零的TCP段被传输时，开启 `TCPSender` 的超时重传计时器，使其在RTO毫秒后过期。

4. 值得注意的是，在超时重传计时器中具有三种超时重传阈值：当前剩余时间`RTO`、RTO初始值 `_initial retransmission timeout` 及RTO基础值。RTO基础值为RTO初始值的一定倍数，其在重传发生时翻倍。

   当 `tick` 被调用且当前超时重传计时器已过期时：

   ​	（a） 将最早发送至对端（最低序列号最小）的TCP段重新传输；

   ​	（b）如果当前对端TCP接收器的窗口大小不为零，则递增当前的重传次数，并使得RTO基础值翻倍，并将RTO置为翻倍后的RTO基础值。

 	5. 当有 `Outstanding segment` 被完全确认时（本地TCP接收端接受的`ackno`大于该段中任何字节的绝对序列号时），将RTO和RTO基础值重置为RTO初始值。当所有TCP段均被对端确认时，即 `Outstanding segment` 数量为零时，停止超时重传计时器。最后，将超时重传次数置为零。

在这里，构建了 `Timer` 计时器类以处理超时重传相关的所有任务，其私有成员如下：

```
 18 class Timer{
 19     private:
 20         bool _running{false};
 21 
 22         unsigned int _RTO;
 23 
 24         unsigned int _base_RTO;
 25 
 26         const unsigned int _initial_RTO;
 27 
 28         unsigned int  _consecutive_retransmissions{0};
 29 
 30         std::queue<std::pair<TCPSegment,uint64_t> > _outstandings
 31     public:
 32         Timer(const uint16_t retx_timeout) : _RTO(retx_timeout),_base_RTO(retx_timeout),_initial_    RTO(retx_timeout) {}

 
```

`_running` 记录了当前计时器的开关状态、`_RTO`、`_base_RTO`、`_initial_RTO` 分别记录了超时重传阈值的剩余值、基础值和初始值、`_consecutive_retransmissions` 记录了超时重传次数、`_outstanding` 保存了当前所有 `Outstanding segment` 及其最后一个字节的绝对序列号。

在这里，一些简单的操作私有成员的接口如下：

```
 34         bool running() { return _running; }
 35 
 36         void on() {_running = true;}
 37         void off() {_running = false;}
 38
 39         unsigned int consecutive_retransmissions() const { return _consecutive_retransmissions;}
 40         void push_outstanding(const TCPSegment& seg, const uint64_t end_seqno) { _outstandings.em    place(seg,end_seqno); }
```

`Timer::tick_handle` 用于完成 `TCPSender` 调用 `tick` 时的计时器任务：

```
 16 void Timer::tick_handle(const size_t ticks,const uint16_t window_size,std::queue<TCPSegment>& seg    ments_out){
 17     if(_running){
 18         if(_RTO <= ticks){
 19             segments_out.push(_outstandings.front().first);
 20             if(window_size != 0){
 21                 _consecutive_retransmissions++;
 22                 _base_RTO *= 2;
 23             }
 24             _RTO = _base_RTO;
 25         }
 26         else
 27             _RTO -= ticks;
 28     }
 29 }
```

当计时器开启时，判断当前RTO剩余值是否小于等于当前时间的流逝值。如是，则将最早的  `Outstanding segment` 重新传输至对端（利用`_outstandings` 的FIFO特性），并在窗口大小非零时递增超时重传次数，并翻倍RTO基础值，并将RTO剩余值置为该值；否则，则将RTO剩余值递减以反映时间流逝。

`Timer::ack_handle` 用于完成 `TCPSender` 调用 `ack_received`  时的计时器任务：

```
 31 uint64_t Timer::ack_handle(const uint64_t ackno){
 32     uint64_t ret = 0;
 33     _consecutive_retransmissions = 0;
 34     while(!_outstandings.empty()){
 35         uint64_t end_seqno = _outstandings.front().second;
 36         if(end_seqno > ackno)
 37             break;
 38         _RTO = _initial_RTO;
 39         _base_RTO = _initial_RTO;
 40         ret += _outstandings.front().first.length_in_sequence_space();
 41         _outstandings.pop();
 42     }
 43     if(_outstandings.empty())
 44         _running = false;
 45     return ret;
 46 }
```

TCP段是按**序列号越小越早传入**规律传入 `_outstandings` 的，因此  `_outstandings` 中的TCP段始终保持序列号有序状态。因此，在这里从前到后检查 `_outstandings` ，将末尾绝对序列号`end_seqno`小于当前 `ackno` 的TCP段全部驱逐，并记录驱逐的TCP段的总长度作为返回值。如有TCP段被驱逐，则将RTO剩余值和基础值置为RTO初始值。如全部TCP段均被驱逐，则关闭计时器。

### TCPSender主要功能接口

```
 58 void TCPSender::fill_window() {
 59     uint64_t window_size = _window_size > 0 ? static_cast<uint64_t>(_window_size) : 1ul;
 60     uint64_t end_seqno = window_size + _ackno;
 61     while(!_fin_sended && _next_seqno < end_seqno && (!_stream.buffer_empty() || _next_seqno == 0     || _stream.eof())){
 62         _timer.on();
 63         TCPSegment seg;
 64         seg.header().syn = _next_seqno == 0;
 65         seg.header().seqno = wrap(_next_seqno,_isn);
 66         size_t read_len = min(static_cast<size_t>(end_seqno - _next_seqno) - seg.header().syn,_st    ream.buffer_size());
 67         read_len = min(read_len, TCPConfig::MAX_PAYLOAD_SIZE);
 68         Buffer payload(_stream.read(read_len));
 69         seg.payload() = payload;
 70         size_t seg_len = seg.length_in_sequence_space();
 71         if(seg_len < end_seqno - _next_seqno){
 72             seg.header().fin = _stream.eof();
 73             seg_len += _stream.eof();
 74             _fin_sended = _stream.eof();
 75         }
 76         _bytes_in_flight += seg_len;
 77         _next_seqno += seg_len;
 78         _segments_out.push(seg);
 79         _timer.push_outstanding(seg,_next_seqno);
 80     }
 81 }
```

`TCPSender::fill_window()` 将本地用户输入的数据传输至互联网（即传输至 `_segments_out` ）。

**59 - 61行**：在这里，如当前对端TCP接收端窗口大小为0，则在本函数中将其视作为1（如无该特性则本地TCP传输段将永远停止工作），并通过对端TCP接收端窗口大小及`ackno`计算出对端TCP接收端所期望接受的字节的最高绝对序列号 `end_seqno` 。

**61 - 62行**：是否继续进行传输的判断条件有三条：1、`FIN`段是否已经被传输；2、下一个TCP传输段所要传输的字节绝对序列号`_next_seqno`是否小于 `end_seqno`；3、当前是否仍存在需要被传输的数据（`SYN`、`FIN` 或实际数据流负载）。如应当继续传输，则开启计时器。

**63 - 69行**：构建当前所需被发送的TCP段，设置`SYN`（根据 `_next_seqno ` 是否为零）、`seqno` 及TCP段负载（从 `Outbound` 数据流中读取）。在这里，TCP段负载的长度为对端的剩余窗口大小`end_seqno - _next_seqno - seg.header().syn `、`Outbound` 数据流中的字节数及TCP段负载最大容量`TCPConfig::MAX_PAYLOAD_SIZE `之中的最小值。

**70 - 75行**：计算当前TCP段的长度 `seg_len` ，如果对端TCP接收端的窗口可以容纳额外的 `FIN` 段，则根据 `Outbound` 的 `eof` 状态设置当前段的`FIN`、并更新 `seg_len` 和 ` _fin_sended` 。

**76 - 79行**：最后，更新 ` _bytes_in_flight` 和 `_next_seqno`，并将该TCP段输入至 `_segments` 和计时器（此时该段为`Outstanding segment`）。

```
 85 void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
 86     uint64_t tmp = unwrap(ackno,_isn,_next_seqno);
 87     if(tmp <= _next_seqno){
 88         _ackno = tmp;
 89         _window_size = window_size;
 90         _bytes_in_flight -= _timer.ack_handle(_ackno);
 91     }
 92 }
```

`TCPSender::ack_received` 通知了对端TCP接收端的`ackno`及窗口大小的更新。首先，将 `ackno` 解压至64位类型，如`_next_seqno` 小于 `ackno` 时（对端接受到了本地尚未发送的字节），则表示接受 `ACK` 异常，直接返回。如无异常情况，则调用计时器的 `ack_handle`，并更新 `_bytes_in_flight` 。

```
 95 void TCPSender::tick(const size_t ms_since_last_tick) {
 96     _timer.tick_handle(ms_since_last_tick,_window_size,_segments_out);
 97 }
```

`TCPSender::tick` 用于表现时间流逝，在这里利用计时器的 `tick_handle` 完成全部工作。

```
101 void TCPSender::send_empty_segment() {
102     TCPSegment seg;
103     seg.header().seqno = wrap(_next_seqno,_isn);
104     _segments_out.push(seg);
105 
106 }
```

`TCPSender::send_empty_segment()` 用于发送一个长度为0的空TCP段，用于TCP发送一些功能段（`ACK`、`RST`）。

### 实验结果

```
[100%] Testing the TCP sender...
Test project /home/xiurui/sponge/bulid
      Start  1: t_wrapping_ints_cmp
1/33 Test  #1: t_wrapping_ints_cmp ..............   Passed    0.00 sec

...
31/33 Test #53: t_address_dt .....................   Passed    0.00 sec
      Start 54: t_parser_dt
32/33 Test #54: t_parser_dt ......................   Passed    0.00 sec
      Start 55: t_socket_dt
33/33 Test #55: t_socket_dt ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 33

Total Test time (real) =   0.86 sec
[100%] Built target check_lab3
```

