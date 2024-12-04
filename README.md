# 算力卡快速入门指南



## 产品简介

芯茧® 人工智能算力卡是 **深圳市云集互联生态科技有限公司** 推出的基于 **AX650N** 芯片的 M.2 2280 计算卡。 
芯茧算力卡在售有两款，4G/8G内存版本，以下表格展示为8G内存的算力卡。

|     产品规格         | 描述                             |
| ------------ | -------------------------------------------------|
| 处理器       | Octa-corre Cortex-A55@1.7GHz    |
| 内存         | 8GB，64bit LPDDR4x                 |
| 存储         | 16MB              |
| 峰值算力  | 18TOPS@INT8 |
| 视频编码     | H.264/H.265，16路 1080P@30fps 编码                     |
| 视频解码     | H.264/H.265，32路 1080P@30fps 解码                      |
| PCIE | PCIE GEN2 X4 |
| 主控CPU支持 | 支持 Intel、AMD、NXP、Xilinx、Raspberry Pi、Rockchip 等 |
| Host 系统    | Ubuntu、Debian、CentOS、麒麟             |
| 外形尺寸     | M.2 2280，M Key                                         |
| 工作电压     | 3.3 V                                                   |
| 整体系统功耗 | ＜8 w                                                   |
| 算法适配 | 支持图像分类，目标检测，自然语言处理，语音网络以及常规大模型 |



##  AXCL 环境搭建



### CentOS 9系统适配

#### 系统信息

```bash
[test@centos ~]$ uname -a
Linux centos 5.14.0-527.el9.x86_64 #1 SMP PREEMPT_DYNAMIC Wed Nov 6 13:28:51 UTC 2024 x86_64 x86_64 x86_64 GNU/Linux
[test@centos ~]$ cat /etc/os-release
NAME="CentOS Stream"
VERSION="9"
ID="centos"
ID_LIKE="rhel fedora"
VERSION_ID="9"
PLATFORM_ID="platform:el9"
PRETTY_NAME="CentOS Stream 9"
ANSI_COLOR="0;31"
LOGO="fedora-logo-icon"
CPE_NAME="cpe:/o:centos:centos:9"
HOME_URL="https://centos.org/"
BUG_REPORT_URL="https://issues.redhat.com/"
REDHAT_SUPPORT_PRODUCT="Red Hat Enterprise Linux 9"
REDHAT_SUPPORT_PRODUCT_VERSION="CentOS Stream"
[test@centos ~]$
```

#### 环境搭建

1. 更新软件包：`sudo yum update`

2. 安装内核源码：`sudo yum install -y kernel-devel kernel-headers`

3. 修改 `grub` 文件添加 `reserved cma size`，转码卡建议设置为 256MB

   ```bash
   [test@centos ~]$ cat /etc/default/grub
   GRUB_TIMEOUT=5
   GRUB_DISTRIBUTOR="$(sed 's, release .*$,,g' /etc/system-release)"
   GRUB_DEFAULT=saved
   GRUB_DISABLE_SUBMENU=true
   GRUB_TERMINAL_OUTPUT="console"
   GRUB_CMDLINE_LINUX="crashkernel=1G-4G:192M,4G-64G:256M,64G-:512M resume=/dev/mapper/cs_192-swap rd.lvm.lv=cs_192/root rd.lvm.lv=cs_192/swap rhgb quiet cma=256M"
   GRUB_DISABLE_RECOVERY="true"
   GRUB_ENABLE_BLSCFG=true
   [test@centos ~]$
   ```

4. 更新 grub：

   ```bash
   sudo su
   grub2-mkconfig -o /boot/grub2/grub.cfg
   grub2-editenv - set "$(grub2-editenv - list | grep kernelopts) cma=256M"
   grubby --update-kernel=ALL --args="cma=256M"
   ```

5. 关闭 SELinux：

   ```bash
   [test@centos ~]$ cat /etc/selinux/config
   
   # This file controls the state of SELinux on the system.
   # SELINUX= can take one of these three values:
   #     enforcing - SELinux security policy is enforced.
   #     permissive - SELinux prints warnings instead of enforcing.
   #     disabled - No SELinux policy is loaded.
   # See also:
   # https://docs.fedoraproject.org/en-US/quick-docs/getting-started-with-selinux/#getting-started-with-selinux-selinux-states-and-modes
   #
   # NOTE: In earlier Fedora kernel builds, SELINUX=disabled would also
   # fully disable SELinux during boot. If you need a system with SELinux
   # fully disabled instead of SELinux running with no policy loaded, you
   # need to pass selinux=0 to the kernel command line. You can use grubby
   # to persistently set the bootloader to boot with selinux=0:
   #
   #    grubby --update-kernel ALL --args selinux=0
   #
   # To revert back to SELinux enabled:
   #
   #    grubby --update-kernel ALL --remove-args selinux
   #
   SELINUX=disabled
   # SELINUXTYPE= can take one of these three values:
   #     targeted - Targeted processes are protected,
   #     minimum - Modification of targeted policy. Only selected processes are protected.
   #     mls - Multi Level Security protection.
   SELINUXTYPE=targeted
   
   
   [test@centos ~]$
   ```

6. 安装相关软件包：

   ```bash
   sudo yum install -y patch
   sudo yum install -y rpm-build
   sudo yum install -y gcc g++ make
   ```

7. 重启，执行 `reboot` 或 `sudo reboot`

8. `dmesg | grep cma` 查看 CMA reserved 是否设置成功：

   ```bash
   [test@centos ~]$ dmesg | grep cma
   [    0.000000] Command line: BOOT_IMAGE=(hd0,gpt2)/vmlinuz-5.14.0-527.el9.x86_64 root=/dev/mapper/cs_192-root ro crashkernel=1G-4G:192M,4G-64G:256M,64G-:512M resume=/dev/mapper/cs_192-swap rd.lvm.lv=cs_192/root rd.lvm.lv=cs_192/swap rhgb quiet cma=256M
   [    0.005347] cma: Reserved 256 MiB at 0x0000000100000000 on node -1
   [    0.014230] Kernel command line: BOOT_IMAGE=(hd0,gpt2)/vmlinuz-5.14.0-527.el9.x86_64 root=/dev/mapper/cs_192-root ro crashkernel=1G-4G:192M,4G-64G:256M,64G-:512M resume=/dev/mapper/cs_192-swap rd.lvm.lv=cs_192/root rd.lvm.lv=cs_192/swap rhgb quiet cma=256M
   [    0.050869] Memory: 1085392K/33116588K available (16384K kernel code, 5720K rwdata, 13156K rodata, 4016K init, 5528K bss, 1080032K reserved, 262144K cma-reserved)
   [    0.198289] cma: Initial CMA usage detected
   [test@centos ~]$
   ```

###  Ubuntu 22.04

#### 系统信息

```bash
test@ubuntu:~$ cat /etc/os-release 
PRETTY_NAME="Ubuntu 22.04.5 LTS"
NAME="Ubuntu"
VERSION_ID="22.04"
VERSION="22.04.5 LTS (Jammy Jellyfish)"
VERSION_CODENAME=jammy
ID=ubuntu
ID_LIKE=debian
HOME_URL="https://www.ubuntu.com/"
SUPPORT_URL="https://help.ubuntu.com/"
BUG_REPORT_URL="https://bugs.launchpad.net/ubuntu/"
PRIVACY_POLICY_URL="https://www.ubuntu.com/legal/terms-and-policies/privacy-policy"
UBUNTU_CODENAME=jammy
test@ubuntu:~$ 
```

#### 3.2.2 环境搭建

>Ubuntu 22.04 默认没有打开 `CONFIG_CMA=y`  和 `CONFIG_DMA_CMA=y`，所以需要重建内核。
>
>使用 `cat /boot/config-$(uname -r) | grep CMA`可以查看当前内核关于CMA的配置。

1. 安装软件包：`sudo apt install gcc g++ make libncurses-dev flex bison libelf-dev libssl-dev`

2. 获取内核源代码：`sudo apt install linux-source-6.5.0`

3. 进入内核源码目录：`cd /usr/src/linux-source-6.5.0/`

4. 为方便操作切换为root用户：`sudo su`

5. 复制内核配置文件：`cp /boot/config-$(uname -r) /usr/src/linux-source-6.5.0/.config`

6. 打开内核配置`CONFIG_CMA=y` 和 `CONFIG_DMA_CMA=y`：可以使用`make menuconfig`也可以使用`vim`编辑`.config`文件。

7. 构建内核 (编译时间较长)：

   ```bash
   make -j$(nproc)
   make modules -j$(nproc)
   make modules_install -j$(nproc)
   make install
   ```

8. 配置CMA大小：`vim /etc/default/grub`，添加 `GRUB_CMDLINE_LINUX="cma=256MB"`

   ```bash
   root@ubuntu:~# cat /etc/default/grub
   # If you change this file, run 'update-grub' afterwards to update
   # /boot/grub/grub.cfg.
   # For full documentation of the options in this file, see:
   #   info -f grub -n 'Simple configuration'
   
   GRUB_DEFAULT=0
   GRUB_TIMEOUT_STYLE=hidden
   GRUB_TIMEOUT=0
   GRUB_DISTRIBUTOR=`lsb_release -i -s 2> /dev/null || echo Debian`
   GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
   GRUB_CMDLINE_LINUX="cma=256MB"
   
   # Uncomment to enable BadRAM filtering, modify to suit your needs
   # This works with Linux (no patch required) and with any kernel that obtains
   # the memory map information from GRUB (GNU Mach, kernel of FreeBSD ...)
   #GRUB_BADRAM="0x01234567,0xfefefefe,0x89abcdef,0xefefefef"
   
   # Uncomment to disable graphical terminal (grub-pc only)
   #GRUB_TERMINAL=console
   
   # The resolution used on graphical terminal
   # note that you can use only modes which your graphic card supports via VBE
   # you can see them in real GRUB with the command `vbeinfo'
   #GRUB_GFXMODE=640x480
   
   # Uncomment if you don't want GRUB to pass "root=UUID=xxx" parameter to Linux
   #GRUB_DISABLE_LINUX_UUID=true
   
   # Uncomment to disable generation of recovery mode menu entries
   #GRUB_DISABLE_RECOVERY="true"
   
   # Uncomment to get a beep at grub start
   #GRUB_INIT_TUNE="480 440 1"
   root@ubuntu:~#
   ```

9. 切换内核版本：`update-initramfs -c -k 6.5.13`

10. 更新grub：`update-grub` 

11. 重启设备：`reboot`

12. 查看CMA reserved的大小：`sudo dmesg | grep cma`

    ```bash
    test@ubuntu:~$ sudo dmesg | grep cma
    [    0.000000] Command line: BOOT_IMAGE=/boot/vmlinuz-6.5.13 root=UUID=ed2bd69d-f89a-4952-baef-aee37d6b02e2 ro cma=256MB quiet splash vt.handoff=7
    [    0.005136] cma: Reserved 256 MiB at 0x0000000100000000
    [    0.087585] Kernel command line: BOOT_IMAGE=/boot/vmlinuz-6.5.13 root=UUID=ed2bd69d-f89a-4952-baef-aee37d6b02e2 ro cma=256MB quiet splash vt.handoff=7
    [    0.198353] Memory: 31551428K/33116588K available (20480K kernel code, 4267K rwdata, 7276K rodata, 4772K init, 17416K bss, 1302756K reserved, 262144K cma-reserved)
    test@ubuntu:~$ 
    ```
### Other
暂时列举centos9和ubuntu22.04, 其他host系统以及各种版本后续加入进来。

## AXCL 驱动安装

暂时使用axcl手动编译，后续会使用针对ubuntu，centos等提供deb和rpm包直接安装驱动。

### 驱动编译

1. 解压源码包：`tar zxvf axcl.tar.gz`
2. 进入 axcl 源码目录：`cd axcl`
3. 添加驱动补丁(首一次编译需要)：`cd drv/pcie/driver/; patch -p3 < pcie_proc.patch; cd -`
4. 编译驱动源码：`make host=x86 clean all install -C drv/pcie/driver`
5. 驱动安装路径：`out/axcl_linux_x86/ko/`

```bash
<axcl> $ ls out/axcl_linux_x86/ko/
axcl_host.ko  ax_pcie_host_dev.ko  ax_pcie_mmb.ko  ax_pcie_msg.ko  ax_pcie_net_host.ko
<axcl> $
```

### 驱动加载

1. 安装算力卡固件：

   ```bash
   sudo mkdir -p /lib/firmware/axcl
   sudo cp -rf image/ax650_card.pac /lib/firmware/axcl/
   ```

2. 加载驱动(启动算力卡)：

   ```bash
   sudo insmod out/axcl_linux_x86/ko/ax_pcie_host_dev.ko
   sudo insmod out/axcl_linux_x86/ko/ax_pcie_msg.ko
   sudo insmod out/axcl_linux_x86/ko/ax_pcie_mmb.ko
   sudo insmod out/axcl_linux_x86/ko/axcl_host.ko
   sudo chmod 666 /dev/msg_userdev
   sudo chmod 666 /dev/ax_mmb_dev
   sudo chmod 666 /dev/axcl_host
   ```

## AXCL 源码编译

```bash
cd <axcl>/build
make host=x86 clean && make host=x86 all install -j8
# 编译后的文件目录 <axcl>/out/axcl_linux_x86
# <axcl>/out/axcl_linux_x86/bin : 存放demo程序
# <axcl>/out/axcl_linux_x86/lib : 存放axcl库

# 添加axcl库的路径
cd <axcl>
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/out/axcl_linux_x86/lib/:$PWD/out/axcl_linux_x86/lib/ffmpeg/
```

## AXCL 快速上手

本节使用的demo可在 `<axcl>/out/axcl_linux_x86/bin`目录找到。

### axcl_sample_runtime

**usage**：

```bash
usage: ./axcl_sample_runtime [options] ... 
options:
  -d, --device    device id (int [=0])
      --json      axcl.json path (string [=./axcl.json])
  -?, --help      print this message

-d: EP slot id, if 0, means conntect to 1st detected EP id
--json: axcl.json file path
```

**example**：

```bash
[test@centos bin]$ ./axcl_sample_runtime
[INFO ][                            main][  22]: ============== V2.16.0_20241111130154 sample started Nov 11 2024 13:21:45 ==============

[INFO ][                            main][  34]: json: ./axcl.json
file sink initialization failed: Failed opening file /tmp/axcl/axcl_logs.txt for writing: Permission denied
[INFO ][                            main][  48]: device id: 1
[INFO ][                            main][  78]: ============== V2.16.0_20241111130154 sample exited Nov 11 2024 13:21:45 ==============

[test@centos bin]$
```

### axcl_sample_memory

**usage**：

```bash
usage: ./axcl_sample_memory [options] ... 
options:
  -d, --device    device id (int [=0])
      --json      axcl.json path (string [=./axcl.json])
  -?, --help      print this message

-d: EP slot id, if 0, means conntect to 1st detected EP id
--json: axcl.json file path
```

**example**：

```bash
[test@centos bin]$ ./axcl_sample_memory
[INFO ][                            main][  32]: ============== V2.16.0_20241111130154 sample started Nov 11 2024 13:21:46 ==============

[INFO ][                           setup][ 112]: json: ./axcl.json
file sink initialization failed: Failed opening file /tmp/axcl/axcl_logs.txt for writing: Permission denied
[INFO ][                           setup][ 127]: device id: 1
[INFO ][                            main][  51]: alloc host and device memory, size: 0x800000
[INFO ][                            main][  63]: memory [0]: host 0x7f52f2ffe010, device 0x18527b000
[INFO ][                            main][  63]: memory [1]: host 0x7f52f27fd010, device 0x185a7b000
[INFO ][                            main][  69]: memcpy from host memory[0] 0x7f52f2ffe010 to device memory[0] 0x18527b000
[INFO ][                            main][  75]: memcpy device memory[0] 0x18527b000 to device memory[1] 0x185a7b000
[INFO ][                            main][  81]: memcpy device memory[1] 0x185a7b000 to host memory[0] 0x7f52f27fd010
[INFO ][                            main][  88]: compare host memory[0] 0x7f52f2ffe010 and host memory[1] 0x7f52f27fd010 success
[INFO ][                         cleanup][ 142]: deactive device 1 and cleanup axcl
[INFO ][                            main][ 106]: ============== V2.16.0_20241111130154 sample exited Nov 11 2024 13:21:46 ==============

[test@centos bin]$
```

### transcode sample (PPL: VDEC - IVPS - VENC)

**usage**：

```bash
usage: ./axcl_sample_transcode --url=string --device=int [options] ... 
options:
  -i, --url       mp4|.264|.265 file path (string)
  -d, --device    device id (int)
      --json      axcl.json path (string [=./axcl.json])
      --loop      1: loop demux for local file  0: no loop(default) (int [=0])
      --dump      dump file path (string [=])
  -?, --help      print this message

-d: EP slot id
--json: axcl.json file path
-i: mp4|.264|.265 file path
--loop: loop to transcode local file until CTRL+C to quit
--dump: dump encoded nalu to local file
```

**example**：

```bash
# transcode input 1080P@30fps 264 to 1080P@30fps 265, save into /tmp/axcl/transcode.dump.pidxxx file.
./axcl_sample_transcode -i bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4 -d 1 --dump /tmp/axcl/transcode.265
[INFO ][                            main][  65]: ============== V2.16.0 sample started Nov  7 2024 16:40:05 pid 1623 ==============

[WARN ][                            main][  85]: if enable dump, disable loop automatically
[INFO ][             ffmpeg_init_demuxer][ 415]: [1623] url: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4
[INFO ][             ffmpeg_init_demuxer][ 478]: [1623] url bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: codec 96, 1920x1080, fps 30
[INFO ][         ffmpeg_set_demuxer_attr][ 547]: [1623] set ffmpeg.demux.file.frc to 1
[INFO ][         ffmpeg_set_demuxer_attr][ 550]: [1623] set ffmpeg.demux.file.loop to 0
[INFO ][                            main][ 173]: pid 1623: [vdec 00] - [ivps -1] - [venc 00]
[INFO ][                            main][ 191]: pid 1623: VDEC attr ==> blk cnt: 8, fifo depth: out 4
[INFO ][                            main][ 192]: pid 1623: IVPS attr ==> blk cnt: 4, fifo depth: in 4, out 0, engine 3
[INFO ][                            main][ 194]: pid 1623: VENC attr ==> fifo depth: in 4, out 4
[INFO ][          ffmpeg_dispatch_thread][ 180]: [1623] +++
[INFO ][             ffmpeg_demux_thread][ 280]: [1623] +++
[INFO ][             ffmpeg_demux_thread][ 316]: [1623] reach eof
[INFO ][             ffmpeg_demux_thread][ 411]: [1623] demuxed    total 470 frames ---
[INFO ][          ffmpeg_dispatch_thread][ 257]: [1623] dispatched total 470 frames ---
[INFO ][                            main][ 223]: ffmpeg (pid 1623) demux eof
[2024-11-07 16:51:16.541][1633][W][axclite-venc-dispatch][dispatch_thread][44]: no stream in veChn 0 fifo
[INFO ][                            main][ 240]: total transcoded frames: 470
[INFO ][                            main][ 241]: ============== V2.16.0 sample exited Nov  7 2024 16:40:05 pid 1623 ==============
```

## AXCL FAQ

### 1. 抓取设备侧日志

**axcl_smi** 工具支持将设备侧的日志（内核和SDK的syslog，运行时库日志）抓取到Host本地，使用方法如下：

```bash
  ./axcl_smi COMMAND {OPTIONS}

  OPTIONS:

      commands
        log                               dump log from slave
      arguments for <log> command
        -d[device], --device=[device]     device, default 0
        -t[type], --type=[type]           type: -1: all, bit mask: 0x01: daemon;
                                          0x02: worker; 0x10: syslog; 0x20:
                                          kernel
        -o[output], --output=[output]     output
```

示例如下：

```bash
[test@centos bin]$ ./axcl_smi log -t -1
command is log
log type is -1
file sink initialization failed: Failed opening file /tmp/axcl/axcl_logs.txt for writing: Permission denied
[11-20 18:13:51:899][E][axcl_smi][main][ 117]: device id: 1
[2024-11-20 18:13:52.356][290043][C][control][dump][72]: log dump finished: ./log_20241120181346.tar.gz
[test@centos bin]$
```

