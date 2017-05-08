/*************************************************************************
	> File Name: timetcpcli.c
	> Author: 
	> Mail: 
	> Created Time: 日  5/ 7 14:35:54 2017
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../unpv13e/lib/unp.h"
/*
 * 在填值的时候需要使用sockaddr_in函数，作为函数参数需要将其强制转换为sockaddr

struct sockaddr{
   uint8_t         sa_len;
   sa_family_t     sa_family;          
   char            sa_data[14];   
}
*/

/*
struct in_addr{
    in_addr_t   s_addr; //历史遗留结构，导致存放地址为 saddr.sin_addr.s_addr
}

struct sockaddr_in
{
    uint8_t         sin_len;
    sa_family_t     sin_family;
    in_port_t       sin_port;                     
    struct in_addr  sin_addr;
    
    unsigned char sin_zero[8];     
    字符数组sin_zero[8]的存在是为了保证结构体struct sockaddr_in的大小和结构体struct sockaddr的大小相等
    没有使用它
};
*/


// 1、创建套接字句柄
// 2、初始化，设置域，端口，以及转换IP地址
// 3、连接
// 4、其他
int main(int argc,char *argv[])
{
    int sockfd,n;
    char recvline[MAXLINE + 1];
    struct sockaddr_in servaddr;

    if(argc != 2) {
        printf("usage error");
        exit(0);
    }
    //socket的通常用法,0是使用默认协议
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        err_sys("socket error");
    /*bzero()将内存块（字符串）的前n个字节清零
    * memset(void *s,int c,size_t n):将s中的前n个字节设置为0
    * bzero不是标准ANSI函数，但是支持socket的基本都支持，而且更好记
    * */
    bezro(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    //htons:host to net short
    //主机字节序转换为网络字节序，short类型
    //short表示16位
    //long表示32位，尽管是64位机器也是这样。历史遗留
    servaddr.sin_port = htons(13);
    if(inet_pton(AF_INET,argv[1],&servaddr.sin_addr) <= 0)
        err_quit("inet_pton error");
    
    /* #define (struct sockaddr) SA
    * 每个套接字函数需要指向一个指向某个套接字结构的指针时，该指针必须强制类型转换为
    * 指向通用套接字地址结构的指针。当时开发这些函数时候，还没有void *通用指针类型
    * */
    if(connect(sockfd,(SA *)&servaddr,sizeof(servaddr)) < 0)
        err_sys("connect error");

    /*tcp:无记录边界的字节流协议。返回的字节可能通过过个PDU(protocol data uint)返回
    * read返回0（表明对端关闭连接）
    * 负值（发生错误）
    * */
    while((n = read(sockfd,recvline,MAXLINE)) > 0)
    {
        recvline[n] = 0;
        //输出到标准输出
        if(fputs(recvline,stdout) == EOF)
            err_sys("fputs error");
    }

    if(n < 0)
        err_sys("read error");
    exit(0);
}
