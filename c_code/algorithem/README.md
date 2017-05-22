## 约瑟夫环的问题：

N个人从1到N排序，围城圆圈。某标志经过M次传递后，握有该标志的人淘汰。该标志后的人继续。比如：M = 0，N=5，那么依次清除，5号获胜；M=1，N=5，则清除序列为2,4,1,5.

使用循环链表解决。实现见josephus.c;

![](http://ww3.sinaimg.cn/large/006HJ39wgy1fft8wfrxjmj30ci06yaaj.jpg)

进一步思考：