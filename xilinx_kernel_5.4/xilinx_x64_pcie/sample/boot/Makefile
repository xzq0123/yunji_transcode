objects = sample_pcie_boot.o

CFLAGS = -g -Wall -O0 -I../include -I ./zlib/ -L ./zlib/lib/

sample_pcie_boot : $(objects)
	aarch64-linux-gnu-gcc $(CFLAGS) -o sample_pcie_boot $(objects)  zlib/*.c -lpthread -lz

# 规则：如何编译 .c 文件生成 .o 文件
%.o: %.c
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

.PHONY : clean
clean :
	rm -f sample_pcie_boot *.o
	rm -f zlib/*.o
