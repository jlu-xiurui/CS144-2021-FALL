# Lab 7

本实验的内容即为将之前所有实验的内容组装起来，检查其是否可以作为一个正常的网络协议栈在互联网中通讯。

## 实验结果

实验指导书中要求与其余同学的网络协议栈相互通讯，由于本人为自学者，因此只能在本机上检验网络协议栈是否工作正常：

![lab7_figure1](C:\Users\xiurui\Desktop\计算机书单\CS144\lab7_figure1.png)

可见网络协议栈工作正常，运行总体测试用例 `make check` 结果如下：

```
        Start 165: t_isnR_128K_8K_L
160/164 Test #165: t_isnR_128K_8K_L .................   Passed    0.26 sec
        Start 166: t_isnR_128K_8K_lL
161/164 Test #166: t_isnR_128K_8K_lL ................   Passed    4.16 sec
        Start 167: t_isnD_128K_8K_l
162/164 Test #167: t_isnD_128K_8K_l .................   Passed    0.47 sec
        Start 168: t_isnD_128K_8K_L
163/164 Test #168: t_isnD_128K_8K_L .................   Passed    0.37 sec
        Start 169: t_isnD_128K_8K_lL
164/164 Test #169: t_isnD_128K_8K_lL ................   Passed    0.52 sec

100% tests passed, 0 tests failed out of 164

Total Test time (real) =  49.20 sec
[100%] Built target check
```

至此，CS144 2021 FALL的实验内容全部完结^ ^。总体而言，本实验的难度并不大（相较于6.828和15213），可以在100小时以内完成，但实验的内容设计的十分巧妙，尤其是前5个实验，让人可以在实践中自我体会TCP架构的设计思路（虽然有一些特性未被实现，如慢启动等，但也无伤大雅），这一点收获是无法从阅读任何书籍的过程中得到的。

