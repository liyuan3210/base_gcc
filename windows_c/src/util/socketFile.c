#include <stdio.h>
#include <winsock2.h>
#include<string.h>
/**
 * 通过socket方式访问
 * 只能读取www.xxx.com/cn/net/...
 * libraries 要添加配置wsock32
 */
int main2()
{
	ReadPage("www.baidu.com");
	printf("over");
    return 0;
}
void ReadPage(char* host)
{
    WSADATA data;
    //winsock版本2.2
    int err = WSAStartup(MAKEWORD(2, 2), &data);
    if (err)
        return ;

    //用域名获取对方主机名
    struct hostent *h = gethostbyname(host);
    if (h == NULL)
        return ;

    //IPV4
    if (h->h_addrtype != AF_INET)
        return ;
    struct in_addr ina;
    //解析IP
    memmove(&ina, h->h_addr, 4);
    LPSTR ipstr = inet_ntoa(ina);

    //Socket封装
    struct sockaddr_in si;
    si.sin_family = AF_INET;
    si.sin_port = htons(80);
    si.sin_addr.S_un.S_addr = inet_addr(ipstr);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sock, (SOCKADDR*)&si, sizeof(si));
    if (sock == -1 || sock == -2)
        return ;

    //发送请求
    char request[1024] = "GET /?st=1 HTTP/1.1\r\nHost:";
    strcat(request, host);
    strcat(request, "\r\nConnection:Close\r\n\r\n");
    int ret = send(sock, request, strlen(request), 0);
    //获取网页内容
    FILE *f = fopen("d:/testPage.txt", "w");
    int isstart = 0;
    while (ret > 0)
    {
        const int bufsize = 1024;
        char* buf = (char*)calloc(bufsize, 1);
        ret = recv(sock, buf, bufsize - 1, 0);
        fprintf(f, "%s", buf);
        printf("buf%s:",buf);
        free(buf);
    }
    fclose(f);
    closesocket(sock);
    WSACleanup();
    printf("读取成功，已保存在testPage.txt中");
    return ;
}
