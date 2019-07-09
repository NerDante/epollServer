// tcp test client, read data from file, send to server
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>

int connect_to_server(char *strAddr)
{
    int sockfd; 
    struct sockaddr_in servaddr; 
    char ipstr[16] = {0};
    unsigned short port;

    //parse address
    sscanf(strAddr, "%[^:]:%hu", ipstr, &port);
    printf("ipstr = %s, port = %hu\n", ipstr, port);
  
    // socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        perror("socket");
        return -1; 
    } 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(ipstr); 
    servaddr.sin_port = htons(port); 
  
    // connect the client socket to server socket 
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) { 
        perror("connect");
        return -1; 
    } 
 
    return sockfd;
}

int send_data(int sockfd, const char *filename)
{   
    int sendlen, len;
    char buf[1024] = {0};
    FILE *fp;

    fp = fopen(filename, "r");
    if(NULL == fp)
    {
        perror("fopen");
        return -1;
    }

    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        printf("read len %d, to send\n", len);
        sendlen = send(sockfd, buf, len, 0);
        if(sendlen < 0)
        {
            perror("send");
            break;
        }
    }
    fclose(fp);
    
    recv(sockfd, buf, sizeof(buf), 0);
    printf("recv data: %s\n", buf);

    return 0;
}

void useAge(const char *name)
{
    printf("%s -s [serverip:serverport] -d [datafile] -t [sendtimes]\n", name);
}

int main(int argc, char *argv[])
{
    int opt, sockfd;
    char serverAddr[32] = {0};
    char inputFile[256] = {0};
    int sendTimes = 0;
    
    if(argc != 7)
    {
        useAge(argv[0]);
        return -1;
    }

    while ((opt = getopt(argc, argv, "s:d:t:h")) != -1) 
    {
        switch (opt) {
            case 's':
                strcpy(serverAddr, optarg);
                break;
            case 'd':
                strcpy(inputFile, optarg);
                break;
            case 't':
                sendTimes = atoi(optarg);
                break;
            case 'h':
                useAge(argv[0]);
                exit(1);
            default:
                printf("parameter is not support\n");
                exit(-1);
        }
    }

    printf("server addr: %s\n", serverAddr);
    printf("input data file: %s\n", inputFile);
    printf("sendTimes: %d\n", sendTimes);

    //connect to server
    sockfd = connect_to_server(serverAddr);
    if(sockfd <= 0)
    {
        return -1;
    }
    
    //send to server
    for(int i = 0; i < sendTimes; i++)
    {
        send_data(sockfd, inputFile);
        usleep(100000);
    }

    return 0;
}
