// the below define is a hack
#define u32 unsigned int
#include "xdma.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define FILEPATH "/dev/xdma"
#define MAP_SIZE  (16000)
#define FILESIZE (MAP_SIZE * sizeof(uint8_t))

uint32_t alloc_offset;
int fd;
uint8_t *map;			/* mmapped array of char's */

int num_of_devices;
struct xdma_dev xdma_devices[MAX_DEVICES];

enum xdma_wait {
	XDMA_WAIT_NONE = 0,
	XDMA_WAIT_SRC = (1 << 0),
	XDMA_WAIT_DST = (1 << 1),
	XDMA_WAIT_BOTH = (1 << 1) | (1 << 0),
};

uint32_t xdma_calc_offset(void *ptr)
{
	return (((uint8_t *) ptr) - &map[0]);
}

uint32_t xdma_calc_size(int length, int byte_num)
{
	length = length * byte_num;

	switch (length % 4) {
	case 3:
		length = (length + 1);
		break;
	case 2:
		length = (length + 2);
		break;
	case 1:
		length = (length + 3);
		break;
	default:
		length = length;
		break;
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

int xdma_init(void)
{
	int i;
	struct xdma_chan_cfg dst_config;
	struct xdma_chan_cfg src_config;

	/* Open a file for writing.
	 */
	fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		return EXIT_FAILURE;
	}

	/* mmap the file to get access to the memory area.
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
			printf
			    ("devices tx chan: %x, tx cmp:%x, rx chan: %x, rx cmp: %x\n",
			     xdma_devices[i].tx_chan, xdma_devices[i].tx_cmp,
			     xdma_devices[i].rx_chan, xdma_devices[i].rx_cmp);

			dst_config.chan = xdma_devices[i].rx_chan;
			dst_config.dir = XDMA_DEV_TO_MEM;
			dst_config.coalesc = 1;
			dst_config.delay = 0;
			dst_config.reset = 0;
			if (ioctl(fd, XDMA_DEVICE_CONTROL, &dst_config) < 0) {
				perror("Error ioctl config rx chan");
				return EXIT_FAILURE;
			}
			printf("config rx chans\n");

			src_config.chan = xdma_devices[i].tx_chan;
			src_config.dir = XDMA_MEM_TO_DEV;
			src_config.coalesc = 1;
			src_config.delay = 0;
			src_config.reset = 0;
			if (ioctl(fd, XDMA_DEVICE_CONTROL, &src_config) < 0) {
				perror("Error ioctl config tx chan");
				return EXIT_FAILURE;
			}
			printf("config tx chans\n");
		}
	}
	return EXIT_SUCCESS;
}

int xdma_exit(void)
{

	/* Don't forget to free the mmapped memory
	 */
	if (munmap(map, FILESIZE) == -1) {
		perror("Error un-mmapping the file");
		return EXIT_FAILURE;
	}

	/* Un-mmaping doesn't close the file.
	 */
	close(fd);
	return EXIT_SUCCESS;
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
	struct xdma_buf_info dst_buf;
	struct xdma_buf_info src_buf;
	struct xdma_transfer dst_trans;
	struct xdma_transfer src_trans;
	const int src_used = ((src_ptr != NULL) && (src_length != 0));
	const int dst_used = ((dst_ptr != NULL) && (dst_length != 0));

	if (device_id >= num_of_devices) {
		perror("Error invalid device ID");
		return -1;
	}

	if (src_used) {
		src_buf.chan = xdma_devices[device_id].tx_chan;
		src_buf.completion = xdma_devices[device_id].tx_cmp;
		src_buf.cookie = (u32) NULL;
		src_buf.buf_offset = (u32) xdma_calc_offset(src_ptr);
		src_buf.buf_size =
		    (u32) xdma_calc_size(src_length, sizeof(src_ptr[0]));

		src_buf.dir = XDMA_MEM_TO_DEV;
		if (ioctl(fd, XDMA_PREP_BUF, &src_buf) < 0) {
			perror("Error ioctl set tx buf");
			return -1;
		}
		printf("config tx buffer\n");
	}

	if (dst_used) {
		dst_buf.chan = xdma_devices[device_id].rx_chan;
		dst_buf.completion = xdma_devices[device_id].rx_cmp;
		dst_buf.cookie = (u32) NULL;
		dst_buf.buf_offset = (u32) xdma_calc_offset(dst_ptr);
		dst_buf.buf_size =
		    (u32) xdma_calc_size(dst_length, sizeof(dst_ptr[0]));

		dst_buf.dir = XDMA_DEV_TO_MEM;
		if (ioctl(fd, XDMA_PREP_BUF, &dst_buf) < 0) {
			perror("Error ioctl set rx buf");
			return -1;
		}
		printf("config rx buffer\n");
	}

	if (src_used) {
		src_trans.chan = xdma_devices[device_id].tx_chan;
		src_trans.completion = xdma_devices[device_id].tx_cmp;
		src_trans.cookie = src_buf.cookie;
		src_trans.wait = (0 != (wait & XDMA_WAIT_SRC));
		if (ioctl(fd, XDMA_START_TRANSFER, &src_trans) < 0) {
			perror("Error ioctl start tx trans");
			return -1;
		}
		printf("config tx trans\n");
	}

	if (dst_used) {
		dst_trans.chan = xdma_devices[device_id].rx_chan;
		dst_trans.completion = xdma_devices[device_id].rx_cmp;
		dst_trans.cookie = dst_buf.cookie;
		dst_trans.wait = (0 != (wait & XDMA_WAIT_DST));
		if (ioctl(fd, XDMA_START_TRANSFER, &dst_trans) < 0) {
			perror("Error ioctl start rx trans");
			return -1;
		}
		printf("config rx trans\n");
	}

	return 0;
}

int main(int argc, char *argv[])
{
	const int LENGTH = 1025;
	int i;
	uint32_t *src;
	uint32_t *dst;

	if (xdma_init() < 0) {
		exit(EXIT_FAILURE);
	}

	dst = (uint32_t *) xdma_alloc(LENGTH, sizeof(uint32_t));
	src = (uint32_t *) xdma_alloc(LENGTH, sizeof(uint32_t));

	printf("src offset %d\n", xdma_calc_offset(src));
	printf("dst offset %d\n", xdma_calc_offset(dst));

	/* Now write int's to the file as if it were memory (an array of ints).
	 */
	// fill src with a value
	for (i = 0; i < LENGTH; i++) {
		src[i] = 'B';
	}
	src[LENGTH - 1] = '\n';

	// fill dst with a value
	for (i = 0; i < LENGTH; i++) {
		dst[i] = 'A';
	}
	dst[LENGTH - 1] = '\n';

	printf("test: dst buffer before transmit:\n");
	for (i = 0; i < 10; i++) {
		printf("%d\t", dst[i]);
	}
	printf("\n");

	xdma_perform_transaction(0, XDMA_WAIT_DST, src, LENGTH, dst, LENGTH);

	printf("test: dst buffer after transmit:\n");
	for (i = 0; i < 10; i++) {
		printf("%d\t", dst[i]);
	}
	printf("\n");

#if 0
	for (i = 0; i < MAP_SIZE; i++) {
		printf("%d\t", map[i]);
	}
	printf("\n");
#endif

	xdma_exit();

	return 0;
}
