#include "libxdma.h"

// the below defines are a hack that enables the use of kernel data types
// without having to included standard kernel headers
#define u32 uint32_t
// dma_cookie_t is defined in the kernel header <linux/dmaengine.h>
#define dma_cookie_t int32_t
#include "xdma.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define BUS_IN_BYTES 4
#define BUS_BURST 16

static uint32_t alloc_offset;
static int fd;
static uint8_t *map;		/* mmapped array of char's */

int num_of_devices;
struct xdma_dev xdma_devices[MAX_DEVICES];

uint32_t xdma_calc_offset(void *ptr)
{
	return (((uint8_t *) ptr) - &map[0]);
}

uint32_t xdma_calc_size(int length, int byte_num)
{
	const int block = (BUS_IN_BYTES * BUS_BURST);
	length = length * byte_num;

	if (0 != (length % block)) {
		length += (block - (length % block));
	}

	return length;
}

// Static allocator
void *xdma_alloc(int length, int byte_num)
{
	void *array = &map[alloc_offset];

	alloc_offset += xdma_calc_size(length, byte_num);

	return array;
}

void xdma_alloc_reset(void)
{
	alloc_offset = 0;
}

int xdma_init(void)
{
	int i;
	struct xdma_chan_cfg dst_config;
	struct xdma_chan_cfg src_config;

	/* Open the char device file.
	 */
	fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		return EXIT_FAILURE;
	}

	/* mmap the file to get access to the DMA memory area.
	 */
	map = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		perror("Error mmapping the file");
		return EXIT_FAILURE;
	}

	xdma_alloc_reset();

	num_of_devices = xdma_num_of_devices();
	if (num_of_devices <= 0) {
		perror("Error no DMA devices found");
		return EXIT_FAILURE;
	}

	for (i = 0; i < MAX_DEVICES; i++) {
		xdma_devices[i].tx_chan = (u32) NULL;
		xdma_devices[i].tx_cmp = (u32) NULL;
		xdma_devices[i].rx_chan = (u32) NULL;
		xdma_devices[i].rx_cmp = (u32) NULL;
		xdma_devices[i].device_id = i;

		if (i < num_of_devices) {
			if (ioctl(fd, XDMA_GET_DEV_INFO, &xdma_devices[i]) < 0) {
				perror("Error ioctl getting device info");
				return EXIT_FAILURE;
			}

			dst_config.chan = xdma_devices[i].rx_chan;
			dst_config.dir = XDMA_DEV_TO_MEM;
			dst_config.coalesc = 1;
			dst_config.delay = 0;
			dst_config.reset = 0;
			if (ioctl(fd, XDMA_DEVICE_CONTROL, &dst_config) < 0) {
				perror("Error ioctl config dst (rx) chan");
				return EXIT_FAILURE;
			}

			src_config.chan = xdma_devices[i].tx_chan;
			src_config.dir = XDMA_MEM_TO_DEV;
			src_config.coalesc = 1;
			src_config.delay = 0;
			src_config.reset = 0;
			if (ioctl(fd, XDMA_DEVICE_CONTROL, &src_config) < 0) {
				perror("Error ioctl config src (tx) chan");
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}

int xdma_exit(void)
{
	if (munmap(map, FILESIZE) == -1) {
		perror("Error un-mmapping the file");
		return EXIT_FAILURE;
	}

	/* Un-mmaping doesn't close the file.
	 */
	close(fd);
	return EXIT_SUCCESS;
}

/* Query driver for number of devices.
 */
int xdma_num_of_devices(void)
{
	int num_devices = 0;
	if (ioctl(fd, XDMA_GET_NUM_DEVICES, &num_devices) < 0) {
		perror("Error ioctl getting device num");
		return -1;
	}
	return num_devices;
}

/* Perform DMA transaction
 *
 * To perform a one-way transaction set the unused directions pointer to NULL
 * or length to zero.
 */
int xdma_perform_transaction(int device_id, enum xdma_wait wait,
			     uint32_t * src_ptr, uint32_t src_length,
			     uint32_t * dst_ptr, uint32_t dst_length)
{
	int ret = 0;
	struct xdma_buf_info dst_buf;
	struct xdma_buf_info src_buf;
	struct xdma_transfer dst_trans;
	struct xdma_transfer src_trans;
	const bool src_used = ((src_ptr != NULL) && (src_length != 0));
	const bool dst_used = ((dst_ptr != NULL) && (dst_length != 0));

	if (device_id >= num_of_devices) {
		perror("Error invalid device ID");
		return -1;
	}

	if (src_used) {
		src_buf.chan = xdma_devices[device_id].tx_chan;
		src_buf.completion = xdma_devices[device_id].tx_cmp;
		src_buf.cookie = 0;
		src_buf.buf_offset = (u32) xdma_calc_offset(src_ptr);
		src_buf.buf_size = (u32) (src_length * sizeof(src_ptr[0]));
		src_buf.dir = XDMA_MEM_TO_DEV;
		ret = (int)ioctl(fd, XDMA_PREP_BUF, &src_buf);
		if (ret < 0) {
			perror("Error ioctl set src (tx) buf");
			return ret;
		}
	}

	if (dst_used) {
		dst_buf.chan = xdma_devices[device_id].rx_chan;
		dst_buf.completion = xdma_devices[device_id].rx_cmp;
		dst_buf.cookie = 0;
		dst_buf.buf_offset = (u32) xdma_calc_offset(dst_ptr);
		dst_buf.buf_size = (u32) (dst_length * sizeof(dst_ptr[0]));
		dst_buf.dir = XDMA_DEV_TO_MEM;
		ret = (int)ioctl(fd, XDMA_PREP_BUF, &dst_buf);
		if (ret < 0) {
			perror("Error ioctl set dst (rx) buf");
			return ret;
		}
	}

	if (src_used) {
		src_trans.chan = xdma_devices[device_id].tx_chan;
		src_trans.completion = xdma_devices[device_id].tx_cmp;
		src_trans.cookie = src_buf.cookie;
		src_trans.wait = (0 != (wait & XDMA_WAIT_SRC));
		ret = (int)ioctl(fd, XDMA_START_TRANSFER, &src_trans);
		if (ret < 0) {
			perror("Error ioctl start src (tx) trans");
			return ret;
		}
	}

	if (dst_used) {
		dst_trans.chan = xdma_devices[device_id].rx_chan;
		dst_trans.completion = xdma_devices[device_id].rx_cmp;
		dst_trans.cookie = dst_buf.cookie;
		dst_trans.wait = (0 != (wait & XDMA_WAIT_DST));
		ret = (int)ioctl(fd, XDMA_START_TRANSFER, &dst_trans);
		if (ret < 0) {
			perror("Error ioctl start dst (rx) trans");
			return ret;
		}
	}

	return ret;
}

int xdma_stop_transaction(int device_id,
			  uint32_t * src_ptr, uint32_t src_length,
			  uint32_t * dst_ptr, uint32_t dst_length)
{
	int ret = 0;
	struct xdma_transfer dst_trans;
	struct xdma_transfer src_trans;
	const bool src_used = ((src_ptr != NULL) && (src_length != 0));
	const bool dst_used = ((dst_ptr != NULL) && (dst_length != 0));

	if (device_id >= num_of_devices) {
		perror("Error invalid device ID");
		return -1;
	}

	if (src_used) {
		src_trans.chan = xdma_devices[device_id].tx_chan;
		ret = (int)ioctl(fd, XDMA_STOP_TRANSFER, &(src_trans.chan));
		if (ret < 0) {
			perror("Error ioctl stop src (tx) trans");
			return ret;
		}
	}

	if (dst_used) {
		dst_trans.chan = xdma_devices[device_id].rx_chan;
		ret = (int)ioctl(fd, XDMA_STOP_TRANSFER, &(dst_trans.chan));
		if (ret < 0) {
			perror("Error ioctl stop dst (rx) trans");
			return ret;
		}
	}

	return ret;
}
