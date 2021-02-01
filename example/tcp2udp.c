// 项目实际问题，通过tcp收到数据后以udp的方式转发到其他进程处理
#include "epollServer.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

static char g_udpStrIp[32];
static unsigned short g_udpPort;

static int send_udp_data(unsigned char* data, unsigned int dataLen, char* strIp, unsigned short port)
{
	int nFd;
	struct sockaddr_in sSvrAddr;
	int ret = 0;

	if(NULL == data || 0 == dataLen || NULL == strIp|| 0 == port)
	{
		return -1;
	}

	bzero(&sSvrAddr, sizeof(struct sockaddr_in));
	sSvrAddr.sin_family = AF_INET;
	sSvrAddr.sin_addr.s_addr = inet_addr((char*)strIp);
	sSvrAddr.sin_port = htons(port);

	nFd = socket(AF_INET, SOCK_DGRAM, 0);
	if(nFd < 0)
	{
		printf("Create socket failed,  strIp:%s, port:%d, error=%s\n",  strIp, port,strerror(errno));
		return -1;
	}

	if(sendto(nFd, data, dataLen,0,(struct sockaddr*)&sSvrAddr,sizeof(struct sockaddr)) < 0)
	{
		printf("sendto failed,  strIp:%s, port:%d, error=%s\n",  strIp, port,strerror(errno));
		close(nFd);
		return -1;

	}

	close(nFd);
	return 0;
}

// echo callback, just send data back to client
void recv_echo_callback(int cliFd, const char* data, unsigned int len)
{
	const char resp[] = {0x6f,0x6b};

    send_udp_data((char *)data, len, g_udpStrIp, g_udpPort);
	send(cliFd, resp, sizeof(resp), 0);
}

int main(int argc, char* argv[])
{
    epoll_server_t* evs;

    if (argc < 3) {
        printf("Usage: %s [tcp-liston-port] [udp-send-port] <optional  udp-server-addr>\n", argv[0]);
        return -1;
    }

    g_udpPort = atoi(argv[2]);
    if(argc == 4)
    {
        strcpy(g_udpStrIp, argv[3]);
    }
    else
    {
        strcpy(g_udpStrIp, "127.0.0.1");
    }

    evs = epoll_tcp_server_init(atoi(argv[1]), recv_echo_callback, 1024); //tcp
    if (NULL == evs) {
        printf("epoll_server_init failed.\n");
        return -1;
    }

    epoll_server_start(evs); // loop forever, unless exception is occured
    epoll_server_delete(evs);

    return 0;
}
