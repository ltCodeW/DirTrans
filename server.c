#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

#define MAXLINE 4096
#define PORT 11710
#define SEND_SZ (4096)
#define RECV_SZ (1024)
#define FILE_SZ 8192
enum
{
    UPLOAD = 0,
    DOWNLOAD,
};
enum
{
    FNAME_FIN=1,
    FNAME_NFIN,
    FCON_FIN,
    FCON_NFIN,
};
void response(void *confd);
void save_file_to_server(int confd);
void send_file_to_client(int confd);
int walkdir(char *dirpath, char *fileNames);

int main()
{
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    //创建一个TCP的socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf(" create socket error: %s (errno :%d)\n", strerror(errno), errno);
        return 0;
    }

    //先把地址清空，检测任意IP
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    //地址绑定到listenfd
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        printf(" bind socket error: %s (errno :%d)\n", strerror(errno), errno);
        return 0;
    }

    //监听listenfd
    if (listen(listenfd, 10) == -1)
    {
        printf(" listen socket error: %s (errno :%d)\n", strerror(errno), errno);
        return 0;
    }

    printf("====waiting for client's request=======\n");
    //accept 和recv,注意接收字符串添加结束符'\0'
    while (1)
    {

        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
        {
            printf(" accpt socket error: %s (errno :%d)\n", strerror(errno), errno);
            return 0;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, (void *)response, (void *)&connfd);
    }
    close(listenfd);
    return 0;
}

void response(void *arg)
{
    int recv_len = 0;
    char recv_buf[RECV_SZ] = {0}; // 接收缓冲区
    int connfd = *((int *)arg);
    int order = 0;

    recv_len = recv(connfd, recv_buf, RECV_SZ, 0);
    if (recv_len < 0)
    {
        printf("send msg error: %s(errno :%d)\n", strerror(errno), errno);
        return;
    }
    order = recv_buf[0];
    switch (order)
    {
    case (UPLOAD):
        printf("save_file_to_server \n");
        save_file_to_server(connfd);
        break;
    case (DOWNLOAD):
        printf("send_file_to_client \n");
        send_file_to_client(connfd);
        break;
    }
}
//先送文件名，再送文件内容
int sendfile(int confd, char *file, int if_fname_fin)
{
    FILE *fp = fopen(file, "rb");
    int send_len;
    //最后1个位置放标志位，
    char send_buf[SEND_SZ] = {0};

    if(strlen(file)+1 >= SEND_SZ){
        printf("file name too long \n");
        return -1;
    }
    strncat(send_buf, file, strlen(file) + 1);
    send_buf[SEND_SZ - 1] = if_fname_fin;

    send_len = send(confd, send_buf,  SEND_SZ, 0);

    memset(send_buf, 0, SEND_SZ);
    while ((send_len = fread(send_buf, 1, SEND_SZ-1, fp)) > 0)
    {
        if(send_len == SEND_SZ-1){
            send_buf[SEND_SZ - 1] = FCON_NFIN;
            send(confd, send_buf, SEND_SZ, 0);
        }else{
            memset(send_buf + send_len, 0, SEND_SZ - send_len);
            send_buf[SEND_SZ - 1] = FCON_FIN;
            send(confd, send_buf, SEND_SZ, 0);
        }
        
    }
    //发送结束字段
    // memset(send_buf, 0, SEND_SZ);
    // send_buf[SEND_SZ-1] = FCON_FIN;
    // send(confd, send_buf, SEND_SZ, 0);
    printf("finish send %s\n", file);
}
void save_file_to_server(int confd) { ; }
void send_file_to_client(int confd)
{

    int recv_len, send_len;
    char recv_buf[RECV_SZ] = {0};
    char send_buf[SEND_SZ] = {0};
    char path[1024] = {0};
    //要保证可容纳存储路径后的文件路径文件名总长
    char fileNames[FILE_SZ] = {0};
    //收文件路径
    recv_len = recv(confd, recv_buf, RECV_SZ, 0);
    if (recv_len < 0)
    {
        perror("recv");
        return;
    }
    strncpy(path, recv_buf, strlen(recv_buf));
    path[recv_len] = 0;

    //得到文件夹下所有完整路路径，以\n间隔路径名
    printf("path:%s\n", path);
    printf("-------------------------\n");
    walkdir(path, fileNames);
    char *tmp_ftail = fileNames;
    char *tmp_fhead = fileNames;
    while (*tmp_ftail != 0)
    {
        if (*tmp_ftail == '\n')
        {
            *tmp_ftail = 0;
            printf("send tmp_fhead %s \n", tmp_fhead);
            if (*(tmp_ftail+1) == 0){
                sendfile(confd, tmp_fhead, FNAME_FIN);
            }
            else{
                sendfile(confd, tmp_fhead, FNAME_NFIN);
            }
            tmp_fhead = tmp_ftail + 1;
        }
        tmp_ftail++;
    }
    close(confd);
    printf("wancheng\n");
}

int walkdir(char *path, char *files)
{
    DIR *dir;
    struct dirent *ptr;
    int len = 0;
    char deepPath[1024] = {0};
    char fileName[1024] = {0};
    if ((dir = opendir(path)) == NULL)
    {
        perror("error");
        return -1;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) ///current dir OR parrent dir
            continue;
        else if (ptr->d_type == 10) ///link file,ignore
            continue;
        else if (ptr->d_type == 4) ///dir
        {
            strncpy(deepPath, path, strlen(path) + 1);
            strncat(deepPath, "/", 1 + 1);
            strncat(deepPath, ptr->d_name, strlen(ptr->d_name) + 1);
            walkdir(deepPath, files);
        }
        else if (ptr->d_type == 8) ///file
        {
            strncpy(fileName, path, strlen(path) + 1);
            strncat(fileName, "/", 1 + 1);
            strncat(fileName, ptr->d_name, strlen(ptr->d_name) + 1);

            if (strlen(files) + strlen(fileName) + 1 > FILE_SZ)
            {
                printf("too much files! fail to get file name \n");
                memset(files, 0, FILE_SZ);
                return -1;
            }
            strncat(files, fileName, strlen(fileName));
            strncat(files, "\n", 1 + 1);
        }
    }
    closedir(dir);
    return 1;
}
