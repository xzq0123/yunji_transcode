objects = sample_pcie_dma_host.o ax_pcie_dma.o mm_buffer.o ../msg/ax_pcie_msg.o

CFLAGS = -g -Wall -O0 -I../include

sample_pcie_msg_host : $(objects)
	aarch64-linux-gnu-gcc $(CFLAGS) -o sample_pcie_dma_host $(objects) -lpthread

# 规则：如何编译 .c 文件生成 .o 文件
%.o: %.c
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

.PHONY : clean
clean :
	rm -f sample_pcie_dma_host *.o
