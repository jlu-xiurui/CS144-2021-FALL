# Lab 1

在本实验中，需要实现 `StreamReassembler` ，其功能为将若干**带有首字节流索引的子字符串**重组为一个完整的字符串，并传入至其中存放的  `ByteStream`。

## 实验源码及讲解

本实验中的核心函数为：`void push_substring(const std::string &data, const uint64_t index, const bool eof)`。其向`StreamReassembler` 中传入子字符串 `data`，`data` 中的首字符在整个字节流中的索引为 `index`，如果 `eof` 为真，则该子字符串为传入的最后一个子字符串。

当在字节流中位于该子字符串之前的所有字节均被存入 `ByteStream` 中时，`push_substring` 才将传入的子字符串存入 `ByteStream`；否则，则将子字符串暂时存储在 `StreamReassembler`，直到其之前的所有字节均被存入 `ByteStream`。值得注意的是，在 `StreamReassembler` 中暂存的字节数**与**在 `ByteStream` 中存放的字节数之和不得超过 `StreamReassembler` 的容积 `_capacity`。

下面是 `StreamReassembler` 的具体代码实现：

```c++
 12 class StreamReassembler {
 13   private:
 14     // Your code here -- add private members as necessary.
 15 
 16     ByteStream _output;  //!< The reassembled in-order byte stream
 17     size_t _capacity;    //!< The maximum number of bytes
 18     std::map<uint64_t,std::string> _mp;
 19     size_t _nidx;
 20     bool _eof;
 21     size_t _eidx;
 22     size_t _unassembled_bytes;
 ...
```

上述为在 `StreamReassembler` 中添加的私有成员，其中 `_output` 为其所需构造的字节流， `_capacity` 为该结构的容积，`_mp` 为字符串首字节在字节流中的位置为索引，子字符串本身为值的索引表，用于存放暂存在结构中的子字符串，`_nidx` 为字节流中第一个未被存放在 `ByteStream`  中的字节索引，`_eof` 标识当前结构是否收取到了末尾子字符串，`_eidx`为字节流中最后一个字节的索引加一，`_unassembled_bytes` 为暂存在 `StreamReassembler` 结构中的子字符串总长度。

在 `StreamReassembler::push_substring` 中，我们完成将子字符串组装的全部工作：

```c++
 20 void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
 21     if(eof){
 22         _eidx = index + data.size();
 23         _eof = true;
 24     }
```

**21 - 24行**：如调用参数 `eof` 为真，则该子字符串为字节流中的最后一个子字符串，将 `_eof ` 置为真，并将 `_eidx` 设置为当前子字符串索引加当前子字符串长度。

```c++
 25     size_t idx = index;
 26     if(idx <= _nidx){
 27         if(data.size() > _nidx - idx){
 28             _nidx +=_output.write(data.substr(_nidx - idx));
 29             auto iter = _mp.begin();
 30             while(iter != _mp.end() && iter->first <= _nidx){
 31                 if(iter->second.size() > _nidx - iter->first){
 32                     _nidx += _output.write(iter->second.substr(_nidx - iter->first));
 33                 }
 34                 _unassembled_bytes -= iter->second.size();
 35                 _mp.erase(iter);
 36                 iter = _mp.begin();
 37             }
 38         }
 39     }
```

**25 - 39行：**如当前子字符串的首字符索引 `index` 小于等于 `_nidx` 且当前子字符串的末字符索引大于 `_nidx` 时，则在本次调用中会使 `_nidx`增加，在这里向 `_output` 写入 `data` 中字节流索引大于等于`_nidx` 的所有字节，并使得 `_nidx` 增加成功写入的字节数。由于，`_nidx`在本次插入操作中增加，因此暂存在 `StreamReassembler` 中的子字符串可能在本次插入后满足了其插入条件：在该子字符串前的所有字节均被写入 `_output`。

在这里，所有首字节索引小于等于 `_nidx` 的子字符串均满足了插入条件。由于 `_mp` 为基于红黑树的有序索引表，因此我们循环访问 `_mp` 的首元素（其首字节索引总是位于 `_mp` 中最小的）。如该元素满足了插入条件，则将该元素插入至 `_output` 并从 `_mp` 中删除；如当前元素不满足插入条件，则其后续元素也一定不满足插入条件，因此终结循环。值得注意的是，如当前元素的末字节索引小于等于 `_nidx` ，则该元素中的所有字节均已被写入 `_output`，因此直接将该元素从 `_mp` 中删除即可。

![lab1_figure1](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/figure/lab1_figure1.png)

```c++
 40     else{
 41         string str = data;
 42         auto inext = _mp.lower_bound(idx);
 43         // if inext's begin isn't bigger than data's end, 
 44         // merge inext into data and erase inext 
 45         while(inext != _mp.end() && inext->first <= idx + str.size()){
 46             if(inext->first + inext->second.size() > idx + str.size()){
 47                 str += inext->second.substr(idx + str.size() - inext->first);
 48             }
 49             _unassembled_bytes -= inext->second.size();
 50             _mp.erase(inext);
 51             inext = _mp.lower_bound(idx);
 52         }
 53         auto ilast = _mp.lower_bound(idx);
 54         if(!_mp.empty() && ilast != _mp.begin()){
 55             // if ilast's end isn't less than data's begin,
 56             // merge ilast into data and erase ilast
 57             ilast--;
 58             if(ilast->first + ilast->second.size() >= idx){
 59                 if(ilast->first + ilast->second.size() < index + str.size()){
 60                     str = ilast->second + str.substr(ilast->first + ilast->second.size() - idx);
 61                 }
 62                 else{
 63                     str = ilast->second;
 64                 }
 65                 _unassembled_bytes -= ilast->second.size();
 66                 idx = ilast->first;
 67                 _mp.erase(ilast);
 68             }
 69         }
 70         size_t store_size = min(_capacity - _unassembled_bytes - _output.buffer_size(),str.size());
 71         _unassembled_bytes += store_size;
 72         _mp.emplace(idx,str.substr(0,store_size));
 73     }
 74
```

**40 - 74行**：如当前子字符串的首字符索引 `index` 大于 `_nidx` 且当前子字符串的末字符索引大于 `_nidx` 时，则当前子字符串需要被暂存在 `StreamReassembler` 中。在这里，将该子字符串与已经暂存在 `StreamReassembler` 中的子字符串进行合并，`str` 用于存储合并的字符串，并被初始化为 `data` 。在这里，需要按首字节索引大小进行分别讨论（为了易于区分，将已经位于 `_mp` 中的子字符串称为**旧串**，将当前即将要被暂存的子字符串称为**新串**）：

1. 遍历首字节索引大于新串首字节索引的所有旧串，如其首字节索引小于等于新串的末字节索引，则将其合并至`str`，并递减 `_unassembled_bytes`、从 `_mp` 中删除该元素；如其首字节索引大于新串的末字节索引，则其后的元素也一定大于，终结循环。
2. 对于首字节索引小于新串首字节索引的第一个旧串，如其末字节索引是否大于等于新串的首字节索引，则将其合并至`str`，并递减 `_unassembled_bytes`、从 `_mp` 中删除该元素。

在完成了合并操作后，将合并后的子字符串添加到 `_mp` 。值得注意的是，只能向 `_mp` 中添加小于等于当前剩余容积的子字符串（`_mp`中所有子字符串的字符数与`_output`中的字符数之和不能超过最大容量）。

![lab1_figure2](C:\Users\xiurui\Desktop\计算机书单\CS144\lab1_figure2.png)

```c++
 75     if(_eof && _nidx >= _eidx)
 76         _output.end_input();
 77 }
```

**75 - 77行**：如已经收到最后一个子字符串，且已经向 `_output` 中输入字符流中的最后一个字符，则将 `_output` 的 `_eof` 置为真。 

## 实验结果

```
[100%] Testing the stream reassembler...
Test project /home/xiurui/sponge/bulid
      Start 18: t_strm_reassem_single
 1/16 Test #18: t_strm_reassem_single ............   Passed    0.00 sec
      Start 19: t_strm_reassem_seq
 2/16 Test #19: t_strm_reassem_seq ...............   Passed    0.00 sec
      Start 20: t_strm_reassem_dup
 3/16 Test #20: t_strm_reassem_dup ...............   Passed    0.00 sec
      Start 21: t_strm_reassem_holes
 4/16 Test #21: t_strm_reassem_holes .............   Passed    0.00 sec
      Start 22: t_strm_reassem_many
 5/16 Test #22: t_strm_reassem_many ..............   Passed    0.08 sec
      Start 23: t_strm_reassem_overlapping
 6/16 Test #23: t_strm_reassem_overlapping .......   Passed    0.00 sec
      Start 24: t_strm_reassem_win
 7/16 Test #24: t_strm_reassem_win ...............   Passed    0.08 sec
      Start 25: t_strm_reassem_cap
 8/16 Test #25: t_strm_reassem_cap ...............   Passed    0.05 sec
      Start 26: t_byte_stream_construction
 9/16 Test #26: t_byte_stream_construction .......   Passed    0.00 sec
      Start 27: t_byte_stream_one_write
10/16 Test #27: t_byte_stream_one_write ..........   Passed    0.00 sec
      Start 28: t_byte_stream_two_writes
11/16 Test #28: t_byte_stream_two_writes .........   Passed    0.00 sec
      Start 29: t_byte_stream_capacity
12/16 Test #29: t_byte_stream_capacity ...........   Passed    0.29 sec
      Start 30: t_byte_stream_many_writes
13/16 Test #30: t_byte_stream_many_writes ........   Passed    0.01 sec
      Start 53: t_address_dt
14/16 Test #53: t_address_dt .....................   Passed    0.02 sec
      Start 54: t_parser_dt
15/16 Test #54: t_parser_dt ......................   Passed    0.00 sec
      Start 55: t_socket_dt
16/16 Test #55: t_socket_dt ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 16

Total Test time (real) =   0.58 sec
[100%] Built target check_lab1
```

