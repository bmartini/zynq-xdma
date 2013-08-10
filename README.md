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

The driver module can be compiling outside of the Linux kernel source tree. A
variable 'KDIR' in the Makefile is used to point to the kernel source
directory. The default value has it pointing to the default Linux install
location for kernel sources. However, if cross compiling or if the sources are
in a non-default location the value can be overridden using an exported
environmental variable or as an argument passes into the make command.

```bash
cd zynq-xdma/dev/
export KDIR=../../linux-xlnx
make
```

or

```bash
cd zynq-xdma/dev/
make KDIR=../../linux-xlnx
```


## Inserting Module

Use of the driver module requires it to be inserted into the running Linux
kernel. Once inserted it will automatically create a character device file in
'/dev' called '/dev/xdma'. However, the default permissions will not allow
non-root users to read/write to the file. These permissions can be overridden
by installing the udev rule file found in this projects 'util' directory into
the systems '/etc/udev/rules.d/' directory. Alternatively, once the modules
inserted the permissions can be changed manually using 'chmod'.

```bash
sudo cp util/80-xdma.rules /etc/udev/rules.d/
sudo insmod dev/xdma.ko
```

or

```bash
sudo insmod xdma.ko && sudo chmod 666 /dev/xdma
```

To remove the module.

```bash
sudo rmmod xdma
```


## Compiling and Running Demo

The demo application assumes that you have the Zynq PL configured as a DMA
loopback device and that it is being compiled on the Zedboard.

```bash
cd demo
make
```
