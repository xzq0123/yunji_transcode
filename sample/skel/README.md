sample_skel

1）功能说明：
skel文件夹下面的代码, 是爱芯SDK包提供的示例参考代码, 方便客户快速的理解SKEL整个模块的配置流程.

2）使用示例：
usage: ./sample_skel <options> ...
options:
-i,     Input File(yuv)
-r,     Input File Resolution(wxh)(yuv: should be input)
-m,     Models deployment path(path name)
-t,     Repeat times((unsigned int), default=1)
-I,     Interval repeat time((unsigned int)ms, default=0)
-c,     Confidence((float: 0-1), default=0)
-H,     Human track size limit((unsigned int), default=0)
-V,     Vehicle track size limit((unsigned int), default=0)
-C,     Cylcle track size limit((unsigned int), default=0)
-d,     Device id
-e,     Skel detect type((unsigned int), default=1)
                0: detect only
                1: detect + track
-N,     Skel NPU type((unsigned int), default=0(VNPU Disable)
                0: VNPU Disable
                1: STD-VNPU Default
                2: STD-VNPU1
                3: STD-VNPU2
                4: STD-VNPU3
                5: BIG-LITTLE VNPU Default
                6: BIG-LITTLE VNPU1
                7: BIG-LITTLE VNPU2
-p,     Skel PPL((unsigned int), default=1)
                4: AX_SKEL_PPL_HVCP
                5: AX_SKEL_PPL_FACE
-v,     Log level((unsigned int), default=5)
                0: LOG_EMERGENCY_LEVEL
                1: LOG_ALERT_LEVEL
                2: LOG_CRITICAL_LEVEL
                3: LOG_ERROR_LEVEL
                4: LOG_WARN_LEVEL
                5: LOG_NOTICE_LEVEL
                6: LOG_INFO_LEVEL
                7: LOG_DEBUG_LEVEL
-h,     print this message

-i 表示输入的YUV数据的文件名<必须>
-r 表示输入的YUV数据的分辨率,格式: wxh，比如1024x576<如果是输入yuv时必须输入分辨率>
-m 表示模型文件所在路径[不输入时，/opt/etc/axcl/skelModels/full]
-t 表示重复执行次数[默认: 1次]
-I 表示重复执行的时间间隔[默认: 0]
-d 表示子卡Device Id
-e 表示检测策略[默认: 1]
-N 表示运行算法的NPU类型[默认: 0]
-c 表示置信度
-H 表示检测人形数目限制
-V 表示检测机动车数目限制
-C 表示检测非机动车数目限制
-p 表示选择算法[默认: AX_SKEL_PPL_HVCP]
-v 表示选择LOG级别[默认: LOG_NOTICE_LEVEL]
-h 打印帮助信息

注：
    默认模型位置在: /opt/etc/axcl/skelModels/full下

3）测试结果范例
输入是YUV文件：
./axcl_sample_skel -i /opt/data/skel/1024x576_traffic.yuv -r1024x576 -p4
SKEL sample: V0.0.7 build: Oct 25 2024 15:21:54
[AX_SYS_LOG] AX_SYS_Log2ConsoleThread_Start
[N][                            main][1082]: Task infomation:
[N][                            main][1083]:    Input file: /opt/data/skel/1024x576_traffic.yuv
[N][                            main][1084]:    Input file resolution: 1024x576
[N][                            main][1085]:    Repeat times: 1
[N][                            main][1086]: SKEL Init Elapse:
[N][                            main][1087]:    AX_SKEL_Init: 1 ms
[N][                            main][1088]:    AX_SKEL_Create: 151 ms
[N][                            main][1114]: SKEL Process times: 1
[N][                            main][1135]: SKEL Process Elapse:
[N][                            main][1136]:    AX_SKEL_SendFrame: 0 ms
[N][                            main][1150]:    AX_SKEL_GetResult: 7 ms
[N][                            main][1152]: SKEL Process Result:
[N][                            main][1157]:    Object Num: 20
[N][                            main][1176]:            Rect[0] vehicle: [311.202026, 306.804932, 127.864868, 114.940796], Confidence: 0.960578
[N][                            main][1212]:            [0]Point Set Size: 0
[N][                            main][1176]:            Rect[1] vehicle: [502.391327, 381.075287, 139.242828, 142.242157], Confidence: 0.960578
[N][                            main][1212]:            [1]Point Set Size: 0
[N][                            main][1176]:            Rect[2] vehicle: [401.769714, 190.992310, 73.470123, 64.651459], Confidence: 0.898356
[N][                            main][1212]:            [2]Point Set Size: 0
[N][                            main][1176]:            Rect[3] vehicle: [371.902161, 84.903854, 39.173553, 31.960434], Confidence: 0.872581
[N][                            main][1212]:            [3]Point Set Size: 0
[N][                            main][1176]:            Rect[4] vehicle: [508.739563, 208.826340, 81.500793, 76.673904], Confidence: 0.857258
[N][                            main][1212]:            [4]Point Set Size: 0
[N][                            main][1176]:            Rect[5] plate: [349.418518, 383.213013, 31.129486, 11.235962], Confidence: 0.853610
[N][                            main][1212]:            [5]Point Set Size: 0
[N][                            main][1176]:            Rect[6] vehicle: [400.975494, 97.153549, 37.594818, 27.277397], Confidence: 0.851829
[N][                            main][1212]:            [6]Point Set Size: 0
[N][                            main][1176]:            Rect[7] vehicle: [525.634216, 58.122829, 49.007019, 43.124653], Confidence: 0.817513
[N][                            main][1212]:            [7]Point Set Size: 0
[N][                            main][1176]:            Rect[8] vehicle: [247.646942, 97.318512, 40.800812, 31.316605], Confidence: 0.817502
[N][                            main][1212]:            [8]Point Set Size: 0
[N][                            main][1176]:            Rect[9] vehicle: [429.634277, 81.446960, 24.357330, 22.164169], Confidence: 0.780962
[N][                            main][1212]:            [9]Point Set Size: 0
[N][                            main][1176]:            Rect[10] plate: [554.988403, 484.923706, 39.978882, 13.596130], Confidence: 0.778573
[N][                            main][1212]:            [10]Point Set Size: 0
[N][                            main][1176]:            Rect[11] vehicle: [768.720642, 132.768646, 73.470154, 49.006989], Confidence: 0.744845
[N][                            main][1212]:            [11]Point Set Size: 0
[N][                            main][1176]:            Rect[12] body: [725.011108, 119.953217, 13.828064, 34.209747], Confidence: 0.744638
[N][                            main][1212]:            [12]Point Set Size: 0
[N][                            main][1176]:            Rect[13] vehicle: [394.752136, 62.020790, 42.495697, 31.960442], Confidence: 0.710655
[N][                            main][1212]:            [13]Point Set Size: 0
[N][                            main][1176]:            Rect[14] vehicle: [286.866638, 98.584488, 36.364899, 27.565262], Confidence: 0.683532
[N][                            main][1212]:            [14]Point Set Size: 0
[N][                            main][1176]:            Rect[15] vehicle: [307.224884, 89.670517, 25.550201, 23.078331], Confidence: 0.645222
[N][                            main][1212]:            [15]Point Set Size: 0
[N][                            main][1176]:            Rect[16] body: [628.619995, 65.410301, 8.342651, 21.274155], Confidence: 0.633626
[N][                            main][1212]:            [16]Point Set Size: 0
[N][                            main][1176]:            Rect[17] vehicle: [564.659180, 34.037788, 10.869202, 11.553448], Confidence: 0.605465
[N][                            main][1212]:            [17]Point Set Size: 0
[N][                            main][1176]:            Rect[18] plate: [539.065186, 267.774445, 21.343262, 9.661530], Confidence: 0.579756
[N][                            main][1212]:            [18]Point Set Size: 0
[N][                            main][1176]:            Rect[19] vehicle: [450.553314, 73.006683, 20.552704, 19.421486], Confidence: 0.568361
[N][                            main][1212]:            [19]Point Set Size: 0