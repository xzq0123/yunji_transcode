1）功能说明：
该模块是爱芯SDK包提供的SYS模块示例代码，方便客户快速理解和掌握SYS相关接口的使用。
代码演示了如下功能：
a)非cache类型CMM内存申请和释放
b)cache类型CMM内存申请和释放
c)公共缓存池创建和使用
d)专用缓存池创建和使用
e)绑定关系创建和查询。

options:
  -d, --device    device id (int [=0])
  -?, --help      print this message

-d: EP slot id, if 0, means conntect to 1st detected EP id

2）使用示例:
/opt/bin/axcl/axcl_sample_sys