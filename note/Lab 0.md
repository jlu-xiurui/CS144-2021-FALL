# Lab0

本实验分为三部分：

1. 利用 `telnet` 进行网络通信，体会网络通信的基本流程；
2. 实现一个基于HTTP的GET函数，并将GET到的数据打印，以了解基本的socket编程；
3. 实现一个具有固定容量的FIFO字节流模型，作为后续实验的前置模型。

由于是第一个实验，因此总体难度不大。

## 实验源码及讲解

### 1.webget

在这里第一部分的内容过于简单，在这里不进行介绍。在第二部分中，我们需要实现 `webget` 程序中的 `get_URL` 函数：

```
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
 19     TCPSocket sock;
 20     sock.connect(Address(host,"http"));
 21 
 22     string URL;
 23     URL += "GET " + path + " HTTP/1.1\r\n";
 24     URL += "Host: " + host + "\r\n";
 25     URL += "Connection: close\r\n\r\n";
 26     sock.write(URL);
 27     string readbuf, buf;
 28     
 29     while(!sock.eof()){
 30         sock.read(readbuf);
 31         buf += readbuf;
 32     }
 33     cout<<buf;
 34 
 35 }
```

其利用CS144的TCPSocket套接字，向服务器发送GET请求，并接受所有服务器的响应并输出，总体结构比较简单。值得注意的是，HTTP的每个请求行都需要以`\r\n`结尾，并且整个请求以`\r\n\r\n`结尾。

### 2.ByteStream

在这里， `ByteStream` 是一个具有固定容量的FIFO字节流，从其输入端传入的任何数据，都可以在其输出端以相同的顺序提取。在这里，根据实验的测试例程，可以判断出 `ByteStream` 需要具有如下细节：

1. 需要维护两个变量，分别存储从FIFO中**输出的字节数**及**输出或驱逐的字节数**；
2. 如当前对象被调用 `end_input()`  成员函数，且当前对象中**存储的字节数为0**，则 `eof()` 返回真；
3. 当写入的字节数大于当前对象的剩余容积时，将**容积之外**的需写入字节直接丢弃。

以下是 `	ByteStream` 的代码实现：

在 ` /libsponge/byte_stream.hh` 中，为该类添加新的私有成员。在这里将 `ByteStream` 以循环数组的方式进行实现， `_head` 指向当前FIFO中首个元素的索引、 `_tail` 指向当前FIFO中下一个插入位置的索引、`_sz` 显示了当前FIFO存储的字节数、`_read_bytes` 和 `_write_bytes` 分别对应从FIFO中输入和删除的字节数、`_end_input` 判断输入端是否调用了 `end_input()` 以提示输入完毕。

```
 11 class ByteStream {
 12   private:
 13     // Your code here -- add private members as necessary.
 14 
 15     // Hint: This doesn't need to be a sophisticated data structure at
 16     // all, but if any of your tests are taking longer than a second,
 17     // that's a sign that you probably want to keep exploring
 18     // different approaches.
 19 
 20     bool _end_input;
 21     unsigned _read_bytes;
 22     unsigned _write_bytes;
 23     std::string _buf;
 24     size_t _head;
 25     size_t _tail;
 26     size_t _sz;
 27     const size_t _capacity;
 28     bool _error;  //!< Flag indicating that the stream suffered an error.
...
(2) 
```

在 `/libsponge/byte_stream.cc` 中，需要实现该类的各接口函数：

```
 76 bool ByteStream::input_ended() const { return _end_input; }
 77 
 78 size_t ByteStream::buffer_size() const { return _sz; }
 79 
 80 bool ByteStream::buffer_empty() const { return _sz == 0; }
 81 
 82 bool ByteStream::eof() const { return _end_input && _sz == 0; }
 83 
 84 size_t ByteStream::bytes_written() const { return _write_bytes; }
 85 
 86 size_t ByteStream::bytes_read() const { return _read_bytes; }
 87 
 88 size_t ByteStream::remaining_capacity() const { return _capacity - _sz; }
```

上述函数返回了当前 `ByteStream` 的状态信息，值得注意的是 `eof()` 函数仅当 `_sz` 为0时才返回真。

```
 18 size_t ByteStream::write(const string &data) {
 19     size_t idx = 0;
 20     size_t data_sz = data.size();
 21     while((_tail != _head || _sz != _capacity) && data_sz != idx){
 22        _buf[_tail] = data[idx++];
 23        _tail = (_tail + 1) % _capacity;
 24        _sz++;
 25     }
 26     _write_bytes += idx;
 27     return idx;
 28 }
```

`ByteStream::write` 向FIFO中写入字节，直到 `data` 中的字节被写入完毕或FIFO被充满，在这里利用 `(_tail == _head && _sz == _capacity)`  判断FIFO是否为满。在写入过程中更新 `_tail` 和 `_sz`，并使 `_write_bytes` 增加写入字节数，最后返回写入字节数。

```
 30 //! \param[in] len bytes will be copied from the output side of the buffer
 31 string ByteStream::peek_output(const size_t len) const {
 32     string ret;
 33     size_t peek_len = min(len, _sz);
 34     size_t idx = _head;
 35     while(peek_len--){
 36         ret += _buf[idx];
 37         idx = (idx + 1) % _capacity;
 38     }
 39     return ret;
 40 }
```

`ByteStream::peek_output` 从FIFO中提取但不驱逐字节，提取字节的数量为 `len` 和FIFO剩余字节间的较小值。

```
 43 void ByteStream::pop_output(const size_t len) {
 44     size_t sz = min(len,_sz);
 45     _head = (_head + sz) % _capacity;
 46     _sz -= sz;
 47     _read_bytes += sz;
 48 }   
```

`ByteStream::pop_output` 从FIFO中驱逐字节，当剩余字节小于传入参数时，将驱逐字节数置为剩余字节数。在这里不进行实际元素的清空，而仅将 `_head` 递增 `len` 即可。值得注意的是，在该函数中需要递增 `_read_bytes`。

```
 52 std::string ByteStream::read(const size_t len) {
 53     string ret; 
 54     size_t sz = min(len,_sz);
 55     size_t read_len = 0;
 56     while(read_len++ != sz){
 57         ret += _buf[_head];
 58         _head = (_head + 1) % _capacity;
 59     }
 60     _sz -= sz;
 61     _read_bytes += ret.size();
 62     return ret;
 63 }

```

`ByteStream::read` 从FIFO中驱逐并提取字节，其具体实现即为前两个函数的结合版本。

## 实验结果

```
[100%] Testing Lab 0...
Test project /home/xiurui/sponge/bulid
    Start 26: t_byte_stream_construction
1/9 Test #26: t_byte_stream_construction .......   Passed    0.00 sec
    Start 27: t_byte_stream_one_write
2/9 Test #27: t_byte_stream_one_write ..........   Passed    0.00 sec
    Start 28: t_byte_stream_two_writes
3/9 Test #28: t_byte_stream_two_writes .........   Passed    0.00 sec
    Start 29: t_byte_stream_capacity
4/9 Test #29: t_byte_stream_capacity ...........   Passed    0.35 sec
    Start 30: t_byte_stream_many_writes
5/9 Test #30: t_byte_stream_many_writes ........   Passed    0.02 sec
    Start 31: t_webget
6/9 Test #31: t_webget .........................   Passed    1.69 sec
    Start 53: t_address_dt
7/9 Test #53: t_address_dt .....................   Passed    0.02 sec
    Start 54: t_parser_dt
8/9 Test #54: t_parser_dt ......................   Passed    0.00 sec
    Start 55: t_socket_dt
9/9 Test #55: t_socket_dt ......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 9

Total Test time (real) =   2.12 sec
[100%] Built target check_lab0
```

