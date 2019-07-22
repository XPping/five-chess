#define _GNU_SOURCE 1
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#define BUFFER_SIZE 1500

int main(int argc, char* argv[])
{
    char *ip = "127.0.0.1";
    int port = 8080;
    if(argc > 2){
        ip = argv[1];
        port = atoi(argv[2]);
    }
    
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);
    
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    
    int ret = connect(sock, (struct sockaddr*)&server_address, sizeof(server_address));
    if(ret < 0){
        printf("connect failed\n");
        close(sock);
        return 1;
    }
    
    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sock;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;
    
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    ret = pipe(pipefd);
    assert(ret != -1);
    
    while(1){
        ret = poll(fds, 2, -1);
        if(ret < 0){
            printf("poll failure\n");
            break;
        }
        if(fds[1].revents & POLLRDHUP){
            printf("server close the connection\n");
            break;
        } else if(fds[1].revents & POLLIN){
            memset(read_buf, '\0', sizeof(read_buf));
            recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
            printf("recv:\n");
            printf("%s\n", read_buf);
        }
        if(fds[0].revents & POLLIN){
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], NULL, sock, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }
    close(sock);
    return 0;
}
