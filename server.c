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
#include <poll.h>
#include <errno.h>

#define USER_LIMIT 2
#define CHESS_SIZE 15
#define BUFFER_SIZE 1024

const char *WIN = "You Win!!!";
const char *FAIL = "You fail!!!";
const char *DRAW = "Draw!!!";
const char *MOVE = "You move, please input the coord of chess such as 01-15, and enter the Enter";
const char *WAIT = "Wait for the other side to go first!!!";
const char *OPPO_QUIT = "Your opponent quit!!!";
const char *ERROR_INPUT = "Error input, please input the coord of chess such as 01-15, and enter the Enter";

int chess[CHESS_SIZE][CHESS_SIZE]; // 0 is blank, 1 is first, 2 is second

/*
 * the struct info send to client
 */
struct chess_info{
    char chess[34*36];
    int state;          // 0 is continue, 1 is first win, 2 is second win, 3 is draw 
};

/*
 * the coord of chess 
 */
struct chess_coord{
    int x;
    int y;
};

/*
 * judge the state of chess, who win or loss or draw 
 */
int judge_state()
{
    int i, j;
    bool draw = true;
    for(i=0; i<CHESS_SIZE; i++){
        for(j=0; j<CHESS_SIZE; j++){
            if(chess[i][j] == 0){
                draw = false;
                continue;
            }
            int k, cnt = 1;
            // hor
            for(k=1; (j-k)>=0; k++){
                if(chess[i][j-k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            for(k=1; (j+k)<CHESS_SIZE; k++){
                if(chess[i][j+k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            if(cnt == 5){
                if(chess[i][j] == 1) return 1;
                else return 2;
            }
            // ver
            cnt = 1;
            for(k=1; (i-k)>=0; k++){
                if(chess[i-k][j] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            for(k=1; (i+k)<CHESS_SIZE; k++){
                if(chess[i+k][j] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            if(cnt == 5){
                if(chess[i][j] == 1) return 1;
                else return 2;
            }
            //top_left -> bottom_right
            cnt = 1;
            for(k=1; (i-k)>=0&&(j-k)>=0; k++){
                if(chess[i-k][j-k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            for(k=1; (i+k)<CHESS_SIZE&&(j+k)<CHESS_SIZE; k++){
                if(chess[i+k][j+k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            if(cnt == 5){
                if(chess[i][j] == 1) return 1;
                else return 2;
            }
            // bottom_left -> top_right
            cnt = 1;
            for(k=1; (i+k)<CHESS_SIZE&&(j-k)>=0; k++){
                if(chess[i+k][j-k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            for(k=1; (i-k)>=0&&(j+k)<CHESS_SIZE; k++){
                if(chess[i-k][j+k] == chess[i][j])
                    cnt++;
                else 
                    break;
            }
            if(cnt == 5){
                if(chess[i][j] == 1) return 1;
                else return 2;
            }
        }
    }
    if(draw) return 3;
    return 0;
}

/*
 * chess layout information
 */
void get_chess_info(struct chess_info *info)
{
    memset(info->chess, '\0', 0);
    int i, j;
    // first line: ' '' '' '' '01020304...15' ''\n'
    info->chess[0] = ' ';
    info->chess[1] = ' ';
    info->chess[2] = ' ';
    info->chess[3] = ' ';
    i = 4;
    for(j=1; j<=CHESS_SIZE; j++){
        if(j<10){
            info->chess[i++] = '0';
            info->chess[i++] = j+'0';
        } else {
            info->chess[i++] = '1';
            info->chess[i++] = j-10 + '0';
        }
    }
    info->chess[i++] = ' ';
    info->chess[i++] = '\n';
    // second line: ' '' '' '' ':' ':' '...:' '' ''\n'
    info->chess[i++] = ' ';
    info->chess[i++] = ' ';
    info->chess[i++] = ' ';
    info->chess[i++] = ' ';
    for(j=1; j<=CHESS_SIZE; j++){
        info->chess[i++] = ':';
        info->chess[i++] = ' ';
    }
    info->chess[i++] = ' ';
    info->chess[i++] = '\n';
    // other lines: 01:' '.' '.' '....' '' ''\n'; 02:' '.' '.' '....' '' ''\n';...; 15:' '.' '.' '....' '' ''\n'
    for(int k=1; k<=CHESS_SIZE; k++){
        if(k<10){
            info->chess[i++] = '0';
            info->chess[i++] = k + '0';
        } else {
            info->chess[i++] = '1';
            info->chess[i++] = k - 10 + '0';
        }
        info->chess[i++] = ':';
        info->chess[i++] = ' ';
        for(j=1; j<=CHESS_SIZE; j++){
            if(chess[k-1][j-1] == 0){
                info->chess[i++] = '.';
                info->chess[i++] = ' ';
            } else if(chess[k-1][j-1] == 1){
                info->chess[i++] = '1';
                info->chess[i++] = ' ';
            } else if(chess[k-1][j-1] == 2){
                info->chess[i++] = '2';
                info->chess[i++] = ' ';
            }
        }
        info->chess[i++] = ' ';
        info->chess[i++] = '\n';
    }
    info->chess[i] = '\0';
    // state 
    info->state = judge_state();
}

/*
 * parse the coord in chess from client 
 */
struct chess_coord parse_coord(char *buf)
{
    struct chess_coord coord;
    coord.x = -1;
    coord.y = -1;
    
    int length = strlen(buf);
    if(length != 6) return coord;
    if(buf[0]<'0' || buf[0]>'9' ||
       buf[1]<'0' || buf[1]>'9' ||
       buf[2] != '-'           ||
       buf[3]<'0' || buf[3]>'9' ||
       buf[4]<'0' || buf[4]>'9') return coord;
    
    char x[3], y[3];
    sscanf(buf, "%2s-%2s\n", x, y);
    int xx = atoi(x);
    int yy = atoi(y);
    //printf("%d, %d\n", xx, yy);
    
    if(xx<=0 || xx>CHESS_SIZE || yy<=0 || yy>CHESS_SIZE){
           return coord;
    }
    
    coord.x = xx;
    coord.y = yy;
    
    return coord;
}

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    const int port = 8080;
    memset(chess, 0, sizeof(chess));
    // sock addr struct
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htons(port);

    // build sock, and build, and listen
    int server_sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(server_sock >= 0);
    int ret = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(server_sock, 5);
    assert(ret != -1);
    
    int user_count = 0;
    struct pollfd fds[USER_LIMIT+1];
    for(int i=1; i<=USER_LIMIT; i++){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = server_sock;
    fds[0].events = POLLIN | POLLRDHUP | POLLERR;
    fds[0].revents = 0;
    
    bool start_game = false;     // start game 
    int turn = 1;           // who turn 
    char recv_buf[BUFFER_SIZE];    // recv buffer 
    
    while(1){
        ret = poll(fds, user_count+1, -1);
        if(ret <0){
            printf("poll failure\n");
            break;
        }
        if(fds[0].revents & POLLIN){ // new connection
            struct sockaddr_in client;
            socklen_t client_length = sizeof(client);
            int client_sock = accept(server_sock, (struct sockaddr*)&client, &client_length);
            if(client_sock < 0){
                printf("errno is: %d\n", errno);
            } else if(user_count >= USER_LIMIT){
                const char* info = "too many clients\n";
                send(client_sock, info, strlen(info), 0);
                close(client_sock);
            } else {
                user_count++;
                fds[user_count].fd = client_sock;
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                
                char tmp_buf[20];
                sprintf(tmp_buf, "Your chess id is %d", user_count);
                send(client_sock, tmp_buf, strlen(tmp_buf), 0);
                printf("new client connet: %d\n", user_count);
                
                if(user_count == USER_LIMIT){ // start game
                    start_game = true;
                    turn = 1;
                    struct chess_info info;
                    get_chess_info(&info);
                    for(int i=1; i<=USER_LIMIT; i++){
                        send(fds[i].fd, info.chess, strlen(info.chess), 0);
                        if(i == 1){
                            send(fds[i].fd, MOVE, strlen(MOVE), 0);
                        } else {
                            send(fds[i].fd, WAIT, strlen(WAIT), 0);
                        }
                    }
                }
            }
        } 
        
        for(int i=1; i<=USER_LIMIT; i++){
            if(fds[i].revents & POLLIN){
                memset(recv_buf, '\0', BUFFER_SIZE);
                ret = recv(fds[i].fd, recv_buf, BUFFER_SIZE-1, 0);
                if(start_game && turn == i){
                    // parse the coord in chess from clientzo 
                    // printf("recv_buf: %s, %ld\n", recv_buf, strlen(recv_buf));
                    if((int)strlen(recv_buf) == 0){
                        close(fds[i].fd);
                        send(fds[USER_LIMIT-i+1].fd, OPPO_QUIT, strlen(OPPO_QUIT), 0);
                        sleep(5);
                        close(fds[USER_LIMIT-i+1].fd);
                        start_game = false;
                        printf("client left1\n");
                        user_count = 0;
                        break;
                    }
                    struct chess_coord coord = parse_coord(recv_buf);
                    if(coord.x == -1 || coord.y == -1 || chess[coord.x-1][coord.y-1] != 0){
                        // client input error
                        send(fds[i].fd, ERROR_INPUT, strlen(ERROR_INPUT), 0);
                        printf("client input error\n");
                    } else {
                        chess[coord.x-1][coord.y-1] = i;
                        struct chess_info info;
                        get_chess_info(&info);
                        if(info.state == 0){
                            turn = USER_LIMIT - i + 1;
                            
                            send(fds[1].fd, info.chess, strlen(info.chess), 0);
                            send(fds[2].fd, info.chess, strlen(info.chess), 0);
                            send(fds[turn].fd, MOVE, strlen(MOVE), 0);
                            send(fds[i].fd, WAIT, strlen(WAIT), 0);
                        } else {
                            send(fds[1].fd, info.chess, strlen(info.chess), 0);
                            send(fds[2].fd, info.chess, strlen(info.chess), 0);
                            if(info.state == 1){
                                // first win 
                                send(fds[1].fd, WIN, strlen(WIN), 0);
                                send(fds[2].fd, FAIL, strlen(FAIL), 0);                                
                            } else if(info.state == 2){
                                // second win 
                                send(fds[1].fd, FAIL, strlen(FAIL), 0);
                                send(fds[2].fd, WIN, strlen(WIN), 0);
                            } else if(info.state == 3){
                                // draw 
                                send(fds[1].fd, DRAW, strlen(DRAW), 0);
                                send(fds[2].fd, DRAW, strlen(DRAW), 0);
                            }
                            sleep(5);
                            start_game = false;
                            for(int ii=1; ii<=USER_LIMIT; ii++){
                                close(fds[ii].fd);
                            }
                            user_count = 0;
                            break;
                        }
                    }
                }
            } 
            if(start_game &&((fds[i].revents & POLLRDHUP) || (fds[i].revents&POLLERR))){
            	close(fds[i].fd);
                send(fds[USER_LIMIT-i+1].fd, OPPO_QUIT, strlen(OPPO_QUIT), 0);
                sleep(5);
		        close(fds[USER_LIMIT-i+1].fd);
                start_game = false;
                user_count = 0;
                printf("client left\n");
		        break;
	        } else if(fds[i].revents & POLLERR){
                printf("haha\n");
            }  
        }
    }
    close(server_sock);
}

