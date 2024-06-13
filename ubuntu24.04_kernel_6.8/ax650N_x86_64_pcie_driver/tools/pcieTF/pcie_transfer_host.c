#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include "ax_base_type.h"
#include "ax_pcie_msg_api.h"
#include "ax_pcie_dma_api.h"

#define MAX_TRANSFER_SIZE   0x300000
#define PCIE_TRANSFER_CMD_PORT 3

#define AX_MM_DEV "/dev/ax_mmb_dev"

// ioctl cmd
#define PCIE_BASE       'H'
#define PCIE_DMA_GET_PHY_BASE  _IOW(PCIE_BASE, 3, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMORY   _IOW(PCIE_BASE, 4, unsigned int)

AX_S32 s32AllPciRmtCnt;

AX_VOID help()
{
    printf("usage: pcie_transfile_host [-h] [-b] <targetid> [-s] [-r] <file path>] \n");
    printf("   -h      : display usage\n");
    printf("   -b      : TargetId\n");
    printf("   -s      : specify send file\n");
    printf("   -r      : specify recv file\n");
    printf("   example     : pcie_transfile_host -b 1 -s send_file.txt\n");
    printf("                 pcie_transfile_host -b 1 -r recv_file.txt\n");

    return;
}

AX_S32 MM_BufferInitTF(AX_U8 **MM_VirtualAddr, AX_U64 *MM_PhyBaseAddr, AX_U64 Size)
{
    AX_S32 AX_MM_Fd;
    AX_U64 DmaBufferSize;
    AX_S32  Ret = 0;

    AX_MM_Fd = open(AX_MM_DEV, O_RDWR);
    if (AX_MM_Fd == -1) {
        printf("open %s failed!\n", AX_MM_DEV);
        return AX_FAILURE;
    }

    DmaBufferSize = Size;
    Ret = ioctl(AX_MM_Fd, AX_IOC_PCIe_ALLOC_MEMORY, &DmaBufferSize);
    if (Ret < 0) {
        printf("alloc MM buffer failed\n");
        Ret = AX_FAILURE;
        goto out;
    }

    Ret = ioctl(AX_MM_Fd, PCIE_DMA_GET_PHY_BASE, MM_PhyBaseAddr);
    if (Ret < 0 || *MM_PhyBaseAddr <= 0) {
        printf("get MM buffer base addr failed\n");
        Ret = AX_FAILURE;
        goto out;
    }
    printf("MM buffer physical base addr:%llx, size:%lld\n", *MM_PhyBaseAddr, DmaBufferSize);

    *MM_VirtualAddr = (AX_U8 *)mmap(NULL, DmaBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, AX_MM_Fd, 0);
    if (*MM_VirtualAddr <= 0) {
        printf("mmap fail\n");
        Ret = AX_FAILURE;
        goto out;
    }

    printf("MM buffer virtual addr:%lx\n", (long)(*MM_VirtualAddr));
    printf("initial data in MM buffer: %s\n", *MM_VirtualAddr);

    return AX_MM_Fd;
out:
    close(AX_MM_Fd);
    return Ret;
}

AX_S32 Send_File(AX_CHAR *File, AX_S32 Target, AX_U8 *MM_VirtualAddr, AX_U64 MM_PhyBaseAddr)
{
    AX_S32 FileFd;
    AX_S32 FileSize;
    AX_S32 ReadSize;
    AX_S32 count = 1;
    AX_S32 Ret = AX_SUCCESS;

    FileFd = open(File, O_RDONLY);
    if (FileFd < 0) {
        printf("open file %s failed!\n", File);
        return AX_FAILURE;
    }

    FileSize = lseek(FileFd, 0, SEEK_END);
    lseek(FileFd, 0, SEEK_SET);
    printf("open %s, size:%d bytes\n", File, FileSize);

    while (count) {
        memset(MM_VirtualAddr, 0, MAX_TRANSFER_SIZE);
        count = read(FileFd, MM_VirtualAddr, MAX_TRANSFER_SIZE);
		if (count == -1) {
			printf("read error\n");
            Ret = AX_FAILURE;
			goto out;
		} else if (count == 0) {
            AX_PCIe_SendDataReadyMsg(Target, PCIE_TRANSFER_CMD_PORT, MM_PhyBaseAddr, count);
			break;
		}

        //send a data ready message to ep
        AX_PCIe_SendDataReadyMsg(Target, PCIE_TRANSFER_CMD_PORT, MM_PhyBaseAddr, count);
        if (Ret < 0) {
            printf("send PCIE_DMA_DATA_READY msg failed\n");
            Ret = AX_FAILURE;
            goto out;
        }
        printf("send PCIE_DMA_DATA_READY msg success\n");

        ReadSize = AX_PCIe_WaitReadDoneMsg(Target, PCIE_TRANSFER_CMD_PORT, -1);
        if (ReadSize < 0) {
            printf("wait read done msg failed\n");
            Ret = AX_FAILURE;
            goto out;
        } else if (ReadSize == 0) {
            printf("ep read data failed\n");
            Ret = AX_FAILURE;
            goto out;
        }
    }
out:
    close(FileFd);
    return Ret;
}

AX_S32 Recv_File(AX_CHAR *File, AX_S32 Target, AX_U8 *MM_VirtualAddr, AX_U64 MM_PhyBaseAddr)
{
    AX_S32 FileFd;
    AX_U32 DataSize;
    AX_U64 DstPhyAddr;
    AX_S32 Ret = AX_SUCCESS;

    remove(File);
    FileFd = open(File, O_WRONLY | O_CREAT | O_APPEND);
    if (FileFd < AX_FAILURE) {
        printf("Open file %s failed\n", File);
        return AX_FAILURE;
    }

    while(1) {
        memset(MM_VirtualAddr, 0, MAX_TRANSFER_SIZE);
        Ret = AX_PCIe_WaitWriteDoneMsg(Target, PCIE_TRANSFER_CMD_PORT, &DstPhyAddr, &DataSize, -1);
        if (Ret < 0) {
            break;
        }
        if (DataSize == 0) {
            printf("Recv file transfer finish\n");
            break;
        }

        if (DstPhyAddr != MM_PhyBaseAddr) {
            printf("Dest physical address does not match\n");
            Ret = AX_FAILURE;
            goto out;
        }

        Ret = write(FileFd, MM_VirtualAddr, DataSize);
        if (Ret != DataSize) {
            printf("write file %s failed!\n", File);
            Ret = AX_FAILURE;
            goto out;
        }

        printf("save %d bytes received date to %s success!\n", Ret, File);

        //send a read_done message to ep
        Ret = AX_PCIe_SendReadDoneMsg(Target, PCIE_TRANSFER_CMD_PORT, DataSize);
        if (Ret < 0) {
            printf("send read_done msg failed\n");
            Ret = AX_FAILURE;
            goto out;
        }
        printf("send read_done msg success\n");
    }

out:
    close(FileFd);
    return Ret;
}

AX_S32 AX_PCIe_Sendfile(AX_CHAR *File, AX_S32 TargetId)
{
    AX_U8  *MM_VirtualAddr = NULL;
    AX_U64 MM_PhyBaseAddr = 0;
    AX_S32 AX_MM_Fd;
    AX_U64 DmaBufferSize = MAX_TRANSFER_SIZE;
    AX_S32 Ret = AX_SUCCESS;

    Ret = AX_PCIe_InitRcMsg(1, PCIE_TRANSFER_CMD_PORT);
    if (Ret == -1) {
        printf("init pcie rc msg failed!\n");
        return AX_FAILURE;
    }

    AX_MM_Fd = MM_BufferInitTF(&MM_VirtualAddr, &MM_PhyBaseAddr, DmaBufferSize);
    if (AX_MM_Fd < 0) {
        printf("init RC physical base address failed!\n");
        Ret = AX_MM_Fd;
        goto out;
    }

    Ret = Send_File(File, TargetId, MM_VirtualAddr, MM_PhyBaseAddr);
    if (Ret < AX_FAILURE) {
        printf("Host send file failed\n");
    }

    munmap(&MM_VirtualAddr, DmaBufferSize);
    close(AX_MM_Fd);
out:
    AX_PCIe_CloseMsgPort(TargetId, PCIE_TRANSFER_CMD_PORT);
    return Ret;
}

AX_S32 AX_PCIe_Recvfile(AX_CHAR *File, AX_S32 TargetId)
{
    AX_U8  *MM_VirtualAddr = NULL;
    AX_U64 MM_PhyBaseAddr = 0;
    AX_S32 AX_MM_Fd;
    AX_U64 DmaBufferSize = MAX_TRANSFER_SIZE;
    AX_S32 Ret = AX_SUCCESS;

    Ret = AX_PCIe_InitRcMsg(1, PCIE_TRANSFER_CMD_PORT);
    if (Ret == -1) {
        printf("init pcie rc msg failed!\n");
        return AX_FAILURE;
    }

    AX_MM_Fd = MM_BufferInitTF(&MM_VirtualAddr, &MM_PhyBaseAddr, DmaBufferSize);
    if (AX_MM_Fd < 0) {
        printf("init RC physical base address failed!\n");
        Ret = AX_MM_Fd;
        goto out;
    }

    Ret = AX_PCIe_SendRcPhyBaseAddr(TargetId, PCIE_TRANSFER_CMD_PORT, MM_PhyBaseAddr);
    if (Ret < 0) {
        printf("send RC physical base physical address to EP failed!\n");
        goto mm_handler;
    }

    Ret = Recv_File(File, TargetId, MM_VirtualAddr, MM_PhyBaseAddr);
    if (Ret < AX_FAILURE) {
        printf("Recv slave file failed\n");
    }

mm_handler:
    munmap(&MM_VirtualAddr, DmaBufferSize);
    close(AX_MM_Fd);
out:
    AX_PCIe_CloseMsgPort(TargetId, PCIE_TRANSFER_CMD_PORT);
    return Ret;
}

int main(int argc, char *const argv[])
{
    AX_S32 Opt;
    AX_S32 TargetId = 0;
    AX_S32 Ret = -1;

    while ((Opt = getopt(argc, argv, "b:s:r:h")) != -1) {
        Ret = 0;
        switch (Opt) {
        case 'b':
            if (optarg != NULL) {
                TargetId = strtol(optarg, 0, 16);
                printf("TargetId = %d\n", TargetId);
            } else {
                printf("Get TargetId failed\n");
            }
            break;
        case 's':
            if (optarg != NULL) {
                printf("SendFile = %s\n", optarg);
                AX_PCIe_Sendfile(optarg, TargetId);
            } else{
                printf("Send file not specified\n");
            }
            break;
        case 'r':
            if (optarg != NULL) {
                printf("RendFile = %s\n", optarg);
                AX_PCIe_Recvfile(optarg, TargetId);
            } else {
                printf("Recv file not specified\n");
            }
            break;
        case 'h' :
            help();
            return 0;
        default:
            printf("unknown option.\n");
            help();
            break;
        }
    }

    return Ret;
}