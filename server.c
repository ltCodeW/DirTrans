#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <libgen.h>

#define PORT 11710
#define BUF_SZ (4096)
#define FILE_SZ (8192)
#define LEN_OFFSET (BUF_SZ-5)

//flag
enum
{
    UPLOAD = 1,
    DOWNLOAD,
};
enum
{
    FNAME_FIN = 1,//文件名，非这次传输的最后一个文件
    FNAME_NFIN,   //文件名，这次传输的最后一个文件
    FCON_FIN,     //文件内容，是这个文件的需最后的数据
    FCON_NFIN,    //文件内容，不是这个文件的需最后的数据
    CMD,          //命令
    PATH,         //文件路径
};
//处理client请求
void response(void *confd);
//从client接受文件信息，存储到服务器
void save_file_to_server(int confd);
//从client接受文件名，将文件发送给client
void send_file_to_client(int confd);
//将dirpath文件夹中的所有文件的完整路径存储到fileNames中
//fileNames中的格式各路径用\n间隔，如"path1\npath2...pathn",
int walkdir(char *dirpath, char *fileNames);
//确保dirpath文件夹下可以生成文件
int check_mkpath(char *dirpath);

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

    //printf("waiting for client's request...\n");
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
int check_warp(int len,char* addr){
    int len_insddr = *(int*)addr;
    if(len!=len_insddr){
        printf("len %d,len in addr %d,NOT EQUAL!\n", len, len_insddr);
        return -1;
    }
    return 0;
}
void response(void *arg)
{
    int recv_len = 0;
    char recv_buf[BUF_SZ] = {0}; // 接收缓冲区
    int connfd = *((int *)arg);
    int order = 0;

    recv_len = recv(connfd, recv_buf, BUF_SZ, 0);
    if (recv_len < 0)
    {
        printf("send msg error: %s(errno :%d)\n", strerror(errno), errno);
        return;
    }

    order = recv_buf[0];
    if(recv_buf[BUF_SZ-1]!=CMD){
        printf("command miss\n");
        return ;
    }

    switch (order)
    {
    case (UPLOAD):
       // printf("save_file_to_server \n");
        save_file_to_server(connfd);
        break;
    case (DOWNLOAD):
     //   printf("send_file_to_client \n");
        send_file_to_client(connfd);
        break;
    }
}
//先送文件名，再送文件内容
int sendfile(int confd, char *file, int if_fname_fin)
{
    int content_len;    
    char send_buf[BUF_SZ] = {0};

    FILE *fp = fopen(file, "rb");
    if(strlen(file)+1+5 >= BUF_SZ){
        printf("file name too long \n");
        return -1;
    }

    //send file name
    strncpy(send_buf, file, strlen(file) + 1);
    content_len = strlen(file) + 1;
    send_buf[BUF_SZ - 1] = if_fname_fin;
    strncpy(send_buf + LEN_OFFSET, (char *)&content_len, sizeof(int));
    
    check_warp(content_len,send_buf + LEN_OFFSET);

    send(confd, send_buf,  BUF_SZ, 0);

    memset(send_buf, 0, BUF_SZ);
    while ((content_len = fread(send_buf, 1, BUF_SZ-5, fp)) >= 0)
    {
        if(content_len == BUF_SZ-5){
            send_buf[BUF_SZ - 1] = FCON_NFIN;
        }else{
            send_buf[BUF_SZ - 1] = FCON_FIN;
        }
        strncpy(send_buf + LEN_OFFSET, (char *)&content_len, sizeof(int));
        
        check_warp(content_len,send_buf + LEN_OFFSET);
        
        send(confd, send_buf, BUF_SZ, 0);

        if(send_buf[BUF_SZ - 1] == FCON_FIN)
            break;
    }

}

void send_file_to_client(int confd)
{

    int recv_len, send_len,content_len;
    char recv_buf[BUF_SZ] = {0};
    char send_buf[BUF_SZ] = {0};
    char path[1024] = {0};
    //要保证可容纳存储路径后的文件路径文件名总长
    char fileNames[FILE_SZ] = {0};
    //收文件路径
    recv_len = recv(confd, recv_buf, BUF_SZ, 0);
    strncpy(path, recv_buf, strlen(recv_buf)+1);
    if (recv_len < 0)
    {
        perror("recv");
        return;
    }
    if(recv_buf[BUF_SZ-1]!=PATH){
        printf("path missed\n");
        return ;
    }
    content_len = *(int *)(&recv_buf[LEN_OFFSET]);
    if(check_warp(content_len,recv_buf+LEN_OFFSET)!=0){
        printf("path wrong\n");
        return ;
    }

    //得到文件夹下所有完整路路径，以\n间隔路径名
    walkdir(path, fileNames);
    char *tmp_ftail = fileNames;
    char *tmp_fhead = fileNames;
    while (*tmp_ftail != 0)
    {
        if (*tmp_ftail == '\n')
        {
            *tmp_ftail = 0;
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
int recvfile(int sockfd, char *server_root,  int *final_flg)
{
    int recv_len,content_len;
    char filename[1024] = {0};
    char recv_buf[BUF_SZ] = {0};
    char *rela_path;

    recv_len = recv(sockfd, recv_buf, BUF_SZ, 0);
    if (recv_len < 0)
    {
        perror("recv");
        return -1;
    }

    *final_flg = recv_buf[BUF_SZ - 1];
    content_len = *(int*)(&recv_buf[LEN_OFFSET]);

    check_warp(content_len,recv_buf + LEN_OFFSET);

    if (!(*final_flg == FNAME_NFIN || *final_flg == FNAME_FIN))
    {
        printf("filename miss\n");
        return -1;
    }

    rela_path = recv_buf;
    strncpy(filename, server_root, strlen(server_root) + 1);
    strncat(filename, rela_path, strlen(rela_path) + 1);
    //printf("making %s\n", filename);

    char tmpdirname[1024] = {0};
    strncpy(tmpdirname, filename, strlen(filename) + 1);
    if (check_mkpath(dirname(tmpdirname)) != 0)
    {
        printf("Fail to save file\n");
    }

    FILE *fp = fopen(filename, "wb"); //以二进制方式打开（创建）文件
    if (fp == NULL)
    {
        printf("Cannot open file %s\n", filename);
        exit(0);
    }
    int content_flag = 0;
    while (recv_len = recv(sockfd, recv_buf, BUF_SZ, 0))
    {
        if(recv_len != BUF_SZ){
            printf("send and recv different bytes\n");
            return -1;
        }
        printf("%s recv %d bytes\n",filename,recv_len);
        content_len = *(int *)(&recv_buf[LEN_OFFSET]);
        content_flag = recv_buf[BUF_SZ - 1];

        fwrite(recv_buf, 1, content_len, fp);

        //check_warp(content_len,recv_buf + LEN_OFFSET);

        if(content_flag == FCON_FIN){
            break;
        }else if(content_flag == FCON_NFIN){
            continue;
        }else{
            printf("Unkown flag \n");
            return -1;
        }
    }

    fclose(fp);
   // printf("recv file %s\n",filename);
}
int check_mkpath(char *dirpath)
{
    char dir[1024] = {0};
    strncpy(dir, dirpath, strlen(dirpath) + 1);
    if (access(dirpath, F_OK) != 0)
    {
        if (mkdir(dirpath, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
        {
            dirname(dir);
            check_mkpath(dir);
            mkdir(dirpath, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        return access(dirpath, F_OK);
    }
    return 0;
}
void save_file_to_server(int sockfd) {
    char server_root[] = "/home/lt/testsavedir";
    int flag = 0;
    do
    {
        recvfile(sockfd, server_root, &flag);
    } while (flag==FNAME_NFIN); 
    
 }