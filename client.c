#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <assert.h>

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
#define SEND_SZ 1024
#define RECV_SZ 4096
int save_file_from_server(char *serverip, int port, char *path, char *localpath);
int check_mkpath(char *dirpath);

int main(int argc, char **argv)
{
    char *serverip = "127.0.0.1";
    int port = 11710;
    // char *paths[] = {
    //     "/mnt/d/material/project/buguan/hot_database/data/QT/来流参数.nml",
    //     //  "/mnt/d/material/project/buguan/hot_database/data/MSL",
    //     //  "/mnt/d/material/project/buguan/hot_database/data/STQ",
    //     //  "/mnt/d/material/project/buguan/hot_database/data/YSGJ收费方式",
    //     //  "/mnt/d/material/project/buguan/hot_database/data/压缩拐角"
    // };
    char localpath[] = "/home/lt/testsavedir/";

    char path[] = "/mnt/d/material/project/buguan/hot_database/data/MSL";
    save_file_from_server(serverip, port, path, localpath);
    return 0;
}

int recvfile(int sockfd, char *server_root, char *localpath, int *final_flg)
{
    int recv_len;
    char filename[1024] = {0};
    char recv_buf[RECV_SZ] = {0};
    char *rela_path;

    recv_len = recv(sockfd, recv_buf, RECV_SZ, 0);
    if (recv_len < 0)
    {
        perror("recv");
        return -1;
    }

    *final_flg = recv_buf[RECV_SZ - 1];
    if (*final_flg == FCON_FIN || *final_flg == FCON_NFIN)
    {
        printf("filename miss\n");
        return -1;
    }

    rela_path = recv_buf;
    rela_path += strlen(server_root);
    strncpy(filename, localpath, strlen(localpath) + 1);
    strncat(filename, rela_path, strlen(rela_path) + 1);
    printf("making %s\n", filename);

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

    while(recv_len=recv(sockfd, recv_buf, RECV_SZ, 0)){
        if(recv_len != RECV_SZ){
            printf("send and recv different bytes\n");
            return -1;
        }
        if (recv_buf[RECV_SZ - 1] == FCON_NFIN ){
                fwrite(recv_buf, 1, recv_len-1, fp);
            }
        else if(recv_buf[RECV_SZ - 1] == FCON_FIN){
            //得到最后文件的长
            int idx = RECV_SZ - 2;
            for (; idx >= 0; idx--)
            {
                if(recv_buf[idx]!=0)
                    break;
            }
            fwrite(recv_buf, 1, idx + 1, fp);
            break;
        }
        else
        {
            printf("Unkown error\n");
            return -1;
        }
    }
    fclose(fp);
    //标志标志位
}
//server_root根据实际来改
int save_file_from_server(char *serverip, int port, char *path, char *localpath)
{
    int sockfd;
    char recv_buf[RECV_SZ] = {0};
    char send_buf[SEND_SZ] = {0};
    int recv_len;
    struct sockaddr_in servaddr;

    char localfile[1024] = {0};
    char *fileName;
    char *server_root = "/mnt/d/material/project/buguan/hot_database/data/";

    //创建socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf(" create socket error: %s (errno :%d)\n", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(serverip);

    //连接
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect ");
        return 0;
    }

    //send发送
    //send commod
    send_buf[0] = DOWNLOAD;
    if (send(sockfd, send_buf, SEND_SZ, 0) < 0)
    {
        perror("send cmd");
        return 0;
    }

    // send path
    if (send(sockfd, path, strlen(path) + 1, 0) < 0)
    {
        perror("send path");
        return 0;
    }
    //write data
    int final_file = 0;
    do
    {
        recvfile(sockfd, server_root, localpath, &final_file);
    } while (final_file==FNAME_NFIN); 

    printf("recv file \n");
    close(sockfd);
    return 0;
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