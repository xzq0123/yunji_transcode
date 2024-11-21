# 赛灵思平台移植爱芯和deepx驱动

xilinx_petalinux_kernel_5.4_ax650.tar.gz 是ax650已经编译好的驱动，可以直接加载。  
xilinx_x64_pcie 是可交叉编译爱芯驱动的源文件，需要在Ubuntu 18.04下交叉编译  
xilinx_deepx_driver.tar.gz 是可交叉编译deepx驱动的源文件，也是在Ubuntu 18.04下交叉编译。

### 交叉编译驱动流程

环境：Ubuntu 18.04

需要先编译sdk，然后再交叉编译驱动。

sdk编译环境请参考 course_s0_Xilinx开发环境安装教程.pdf  
sdk编译流程参考： course_s3_ALINX_ZYNQ_MPSoC开发平台Linux基础教程V1.05.pdf  
交叉编译驱动参考：course_s4_ALINX_ZYNQ_MPSoC开发平台Linux驱动教程V1.04.pdf