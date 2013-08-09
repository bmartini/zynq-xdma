# Zynq DMA Linux Driver

This Linux driver has been developed to run on the Xilinx Zynq FPGA. It is a
wrapper driver used to talk to the low level Xilinx driver (xilinx_axidma.c)
that interfaces to a Xilinx DMA Engine implemented in the PL section of the
Zynq FPGA. Userspace applications uses this wrapper driver to configure and
control the DMA operations.


## Compile

Kernel modules need to be built against the version of the kernel it will be
inserted in. It is recommended to uses the Linux kernel maintained by Xilinx.

``` bash
git clone https://github.com/Xilinx/linux-xlnx.git
```

Place the driver directory into the kernel directory 'linux-xlnx/drivers/dma'.
Then, to compile, 'cd' into the driver directory and run 'make'.

```bash
cd linux-xlnx/drivers/dma/zynq-xdma/
make
```


## Inserting Module

To use the driver module it first needs to be inserted into the Linux kernel.

```bash
sudo insmod xdma.ko && sudo chmod 777 /dev/xdma
```

To remove the module.

```bash
sudo rmmod xdma
```


## Compiling and Running Demo

The demo application assumes that you have the Zynq PL configured as a DMA
loopback device.

```bash
gcc -Wall xdma-demo.c -o demo && ./demo
```
