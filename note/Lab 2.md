# Lab 2

在本实验中，我们将完成 `TCPReceiver` 接收器结构，其功能为从互联网接受TCP段（`TCPSegment`），并将其组装成可以读取的数据流 `Inbound`，应用程序可以通过 `Inbound` 数据流从网络中读取数据。

## 实验代码及讲解

### WrappingInt32

为了节省TCP段用于存储序列号的空间，在这里使用 `WrappingInt32` 这一32位的结构压缩原有的64位绝对序列号。为了完成这一功能，我们需要在构建TCP段时将64位序列号压缩，并在解析TCP段时将 `WrappingInt32`  解压。在这里，有三种索引需要进行区分：

![lab2_figure1](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/figure/lab2_figure1.png)

首先是存储在TCP段中的段序列号，其规格为32位，序列号的起始为一个随机的偏移量`ISN`（为了保证数据传输的安全性），且 `SYN` 和 `FIN` 均占据其一位；接着即为数据流的绝对序列号，其规格为64位，起始为0，且 `SYN` 和 `FIN` 均占据其一位；最后为 `ByteStream` 中的数据索引，其与绝对序列号的区别仅为 `SYN` 和 `FIN` 不占据其的位。

 `WrappingInt32` 实质上即为32位无符号整数，并使用 `wrap` 对其进行压缩、`unwrap` 对其进行解压：

```c++
 16 WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { 
 17     uint64_t ret = static_cast<uint64_t>(isn.raw_value());
 18     return WrappingInt32(static_cast<uint32_t>(ret + n));
 19 }
```

压缩过程比较简单，将偏移量 `isn` 转换为64位类型并直接与偏移量 `n` 相加，最后将其转化为 `WrappingInt32` 类型即可，对于64位整数转化为32位整数导致的溢出直接忽略即可。

```c++
 31 uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
 32     uint64_t num = static_cast<uint64_t>(n.raw_value()) - static_cast<uint64_t>(isn.raw_value());
 33     num &= 0xffffffff;
 34     uint64_t base = checkpoint >> 32 << 32;
 35     uint64_t ret = base + num;
 36     if(ret > checkpoint){
 37         if(ret > UINT32_MAX && ret - checkpoint > checkpoint - ret + (1ul<<32))
 38             return ret - (1ul<<32);
 39         return ret;
 40     }
 41     return checkpoint - ret > ret + (1ul<<32) - checkpoint ? ret + (1ul<<32) : ret;
 42 }
```

对于解压过程比较复杂，由于64位整数与32位整数互相转换导致的溢出，一个压缩过的32位整数 `n` 的实际值为 `k(2^31) + n` 其中 `k`为任意整数，因此再解压过程中需要引入 `checkpoint`，并使得解压后的值尽可能地接近 `checkpoint` ，以保证解压过程的唯一性。

在这里，将段序列号与 `ISN` 相减得到绝对序列号的低32位 `num`，并将 `base` 规定为 `checkpoint` 的高32位。显而易见的是，最终的绝对序列号为`num + base`、`num + base - 2^31` 和 `num + base + 2^31` 三者之一。在这里，选取与 `checkpoint` 绝对差最小的绝对序列号返回即可。

### TCPReceiver

`TCPReceiver` 结构的原理比较简单，其解析读入的 `TCPSegment` 中的`SYN`、`FIN`和实际负载`payload`，并利用`StreamReassembler`完成实际的组装工作。为了完成 `TCPReceiver` ，在这里为其添加了如下私有成员：

```c++
 16 class TCPReceiver {
 17     //! Our data structure for re-assembling bytes.
 18     StreamReassembler _reassembler;
 19 
 20     //! The maximum number of bytes we'll store.
 21     size_t _capacity;
 22 
 23     bool _syn_arrived;
 24 
 25     WrappingInt32 _isn;
 26 
 27     uint64_t _ackno;
 28   public:
 29     //! \brief Construct a TCP receiver
 30     //!
 31     //! \param capacity the maximum number of bytes that the receiver will
 32     //!                 store in its buffers at any give time.
 33     TCPReceiver(const size_t capacity) : _reassembler(capacity), _capacity(capacity), _syn_arrive    d(false),_isn(0),_ackno(0) {}
...
```

在这里，`_syn_arrived` 用于指示带有 `SYN` 标识的段是否到来，`_isn` 记录了当前流的初始偏移量、`_ackno` 记录了当前接收器所希望收到的下一个字节的绝对序列号。

```c++
 13 void TCPReceiver::segment_received(const TCPSegment &seg) {
 14     TCPHeader header = seg.header();
 15     if(header.syn){
 16         _syn_arrived = true;
 17         _isn = header.seqno;
 18     }
 19     if(_syn_arrived){
 20         bool eof = header.fin;
 21         uint64_t idx = unwrap(header.seqno,_isn,_ackno);
 22         if((idx == 0 && !header.syn) || idx >= window_size() + _ackno)
 23             return;
 24         _reassembler.push_substring(seg.payload().copy(),idx - 1 + header.syn,eof);
 25         _ackno = _reassembler.next_idx() + _syn_arrived + _reassembler.stream_out().input_ended()    ;   
 26     }
 27 }  
```

`TCPReceiver::segment_received` 为接收器的主要函数，其读入 `TCPSegment` 并对其进行组装。

**14 - 18行**：在这里，首先对段的 `SYN` 进行检查，如其被设置则`_syn_arriver` 被设置为真，且 `_isn` 被设置为该段的序列号 `seqno` 。

**19 - 21行**：如 `SYN` 段尚未到达，则忽略任何到达的段；如 `SYN` 段已到达，则解析段的 `FIN` 作为传入 `_reassembler.push_substring` 的 `eof` 参数。并利用 `unwrap` 获得当前段的绝对偏移量，其 `checkpoint` 被设置为接收器所希望收到的下一个字节的绝对序列号`_ackno` 。

**22 - 25行**：判断该绝对序列号是否合理（当其 `SYN` 未设置但绝对序列号为0、或绝对序列号超出窗口时不合理），如不合理则直接返回。如合理则使用 `_reassembler.push_substring` 组装数据流。值得注意的是，在这里需要将绝对序列号转化为数据流索引，即在`_reassembler` 中忽略`SYN`和`FIN`。最后，将 `_ackno` 更新为下一个等待接受字节的绝对序列号。

```c++
size_t next_idx() const {return _nidx;}
```

在这里，`next_idx` 返回了 `_reassembler`中的`_nidx`（见Lab1笔记），其记录的是 `_reassembler` 中被成功组装的字符串的后一个字节。其为数据流索引，应将其转化为绝对序列号。

```c++
 29 optional<WrappingInt32> TCPReceiver::ackno() const { 
 30     if(!_syn_arrived)
 31         return {};
 32     return wrap(_ackno,_isn);
 33 }
```

`TCPReceiver::ackno` 当 `SYN` 已被接受时，返回段序列号值；否则返回空。

```c++
size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size();}
```

`TCPReceiver::window_size()` 返回当前接收器的窗口大小，即接收器还可以接受的字节数。通过实验指导书及测试用例可以得知，其大小为**接收器容积**减去当前**已被组装的但未被用户读取**的数据流i长度。

## 实验结果

```
Test project /home/xiurui/sponge/bulid
      Start  1: t_wrapping_ints_cmp
 1/26 Test  #1: t_wrapping_ints_cmp ..............   Passed    0.00 sec
      Start  2: t_wrapping_ints_unwrap
 2/26 Test  #2: t_wrapping_ints_unwrap ...........   Passed    0.00 sec
      Start  3: t_wrapping_ints_wrap
 3/26 Test  #3: t_wrapping_ints_wrap .............   Passed    0.00 sec

...
      Start 28: t_byte_stream_two_writes
21/26 Test #28: t_byte_stream_two_writes .........   Passed    0.00 sec
      Start 29: t_byte_stream_capacity
22/26 Test #29: t_byte_stream_capacity ...........   Passed    0.27 sec
      Start 30: t_byte_stream_many_writes
23/26 Test #30: t_byte_stream_many_writes ........   Passed    0.01 sec
      Start 53: t_address_dt
24/26 Test #53: t_address_dt .....................   Passed    0.06 sec
      Start 54: t_parser_dt
25/26 Test #54: t_parser_dt ......................   Passed    0.00 sec
      Start 55: t_socket_dt
26/26 Test #55: t_socket_dt ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 26

Total Test time (real) =   0.81 sec
Built target check_lab2
```

