# mks-tinybee-esp3d-fixed
esp3dlib 存在的问题：使用reprter-host之类的的工具进行tcp连接，进行高速传输数据时会出现传输错误，导致无法正常继续工作。
或者出现lwip的crash故障导致esp32重启。
这几个问题的原因都是esp3dlib没有做多线程互斥访问导致的严重缺陷，以至于几乎无法正常使用。
替换这几个文件将解决此问题，使得esp32恢复可靠的网络连接。
