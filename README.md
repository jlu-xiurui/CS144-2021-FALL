# CS144-2021-FALL
CS144 2021 FALL的实验源码及笔记，实验的官网为[cs144](https://cs144.github.io/)，有关实验的实验指导书和实验代码均可从该网站下载。实验的笔记位于note目录，实验源码位于lab目录。

### 1. Lab0:networking warmup  [Lab0_note](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/note/Lab%200.md) 

  本实验分为三部分：

  1. 利用 `telnet` 进行网络通信，体会网络通信的基本流程；
  2. 实现一个基于HTTP的GET函数，并将GET到的数据打印，以了解基本的socket编程；
  3. 实现一个具有固定容量的FIFO字节流模型，作为后续实验的前置模型。

  由于是第一个实验，因此总体难度不大。

### 2. Lab1:stitching substrings into a byte stream  [Lab1_note](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/note/Lab%201.md) 

  在本实验中，需要实现 `StreamReassembler` ，其功能为将若干**带有首字节流索引的子字符串**重组为一个完整的字符串，并传入至其中存放的  `ByteStream`。

### 3. Lab2:the TCP receiver  [Lab2_note](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/note/Lab%202.md) 

  在本实验中，我们将完成 `TCPReceiver` 接收器结构，其功能为从互联网接受TCP段（`TCPSegment`），并将其组装成可以读取的数据流 `Inbound`，应用程序可以通过 `Inbound` 数据流从网络中读取数据。

### 4. Lab3:the TCP sender  [Lab3_note](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/note/Lab%203.md) 

  在本实验中，需要完成 `TCPSender` 传输器，其将从应用程序输入的 `Outbound` 数据流转化为TCP段，并发送至互联网。

### 5. Lab4: the summit (TCP in full)  [Lab4_note](https://github.com/jlu-xiurui/CS144-2021-FALL/blob/main/note/Lab%204.md) 

  本实验的任务为将之前所有实验的工作进行融合，构建用于TCP链接的基本构件`TCPConnect`链接器。
