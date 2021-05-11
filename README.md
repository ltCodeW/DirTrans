# DirTrans
编译：
    gcc ./server.c -o server -pthread
    gcc ./client.c -o client
运行
    ./server
    ./client
注意：
    1.修改client中的ip(port可以不变)
    2.根据需求修改server和client中的server_root,server_root应该是上传的根目录。

