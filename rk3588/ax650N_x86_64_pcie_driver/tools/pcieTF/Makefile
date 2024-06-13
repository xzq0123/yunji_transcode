objects = pcie_transfer_host.o ../../sample/dma/ax_pcie_dma.o ../../sample/msg/ax_pcie_msg.o

CFLAGS = -g -Wall -O0 -I../../sample/include

sample_pcie_msg_host : $(objects)
	gcc $(CFLAGS) -o pcie_transfer_host $(objects)

.PHONY : clean
clean :
	rm -f pcie_transfer_host *.o