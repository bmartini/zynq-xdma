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
#define MAP_SIZE  (4000)
#define FILESIZE (MAP_SIZE * sizeof(char))

uint32_t alloc_offset;
uint8_t *map;		/* mmapped array of char's */


uint32_t xdma_calc_offset(void *ptr)
{
	return (((uint8_t *) ptr) - &map[0]);
}

uint32_t xdma_calc_size(int length, int byte_num)
{
	length = length * byte_num;

	switch (length % 4) {
	case 3:
		length = (length+1);
		break;
	case 2:
		length = (length+2);
		break;
	case 1:
		length = (length+3);
		break;
	default:
		length = length;
		break;
	}

	return length;
}


uint8_t *xdma_alloc_uint8(int length)
{
	uint8_t *array = &map[alloc_offset];

	switch (length % 4) {
	case 3:
		alloc_offset += (length+1);
		break;
	case 2:
		alloc_offset += (length+2);
		break;
	case 1:
		alloc_offset += (length+3);
		break;
	default:
		alloc_offset += length;
		break;
	}

	return array;
}

int main(int argc, char *argv[])
{
	const int LENGTH = 1024;
	int i;
	int fd;
	uint8_t *src;
	uint8_t *dst;

	/* Open a file for writing.
	 *  - Creating the file if it doesn't exist.
	 *  - Truncating it to 0 size if it already exists. (not really needed)
	 *
	 * Note: "O_WRONLY" mode is not sufficient when mmaping.
	 */
	fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}

	/* mmap the file to get access to the memory area.
	 */
	map = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		perror("Error mmapping the file");
		exit(EXIT_FAILURE);
	}

	alloc_offset = 0;

	dst = xdma_alloc_uint8(LENGTH);
	src = xdma_alloc_uint8(LENGTH);

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

	/* Query driver for number of devices.
	 */
	int num_devices = 0;
	if (ioctl(fd, XDMA_GET_NUM_DEVICES, &num_devices) < 0) {
		perror("Error ioctl getting device num");
		exit(EXIT_FAILURE);
	}
	printf("Number of devices: %d\n", num_devices);

	/* Query driver for number of devices.
	 */
	struct xdma_dev dev;
	dev.tx_chan = (u32) NULL;
	dev.tx_cmp = (u32) NULL;
	dev.rx_chan = (u32) NULL;
	dev.rx_cmp = (u32) NULL;
	dev.device_id = num_devices - 1;
	if (ioctl(fd, XDMA_GET_DEV_INFO, &dev) < 0) {
		perror("Error ioctl getting device info");
		exit(EXIT_FAILURE);
	}
	printf("devices tx chan: %x, tx cmp:%x, rx chan: %x, rx cmp: %x\n",
	       dev.tx_chan, dev.tx_cmp, dev.rx_chan, dev.rx_cmp);

	struct xdma_chan_cfg rx_config;
	rx_config.chan = dev.rx_chan;
	rx_config.dir = XDMA_DEV_TO_MEM;
	rx_config.coalesc = 1;
	rx_config.delay = 0;
	rx_config.reset = 0;
	if (ioctl(fd, XDMA_DEVICE_CONTROL, &rx_config) < 0) {
		perror("Error ioctl config rx chan");
		exit(EXIT_FAILURE);
	}
	printf("config rx chans\n");

	struct xdma_chan_cfg tx_config;
	tx_config.chan = dev.tx_chan;
	tx_config.dir = XDMA_MEM_TO_DEV;
	tx_config.coalesc = 1;
	tx_config.delay = 0;
	tx_config.reset = 0;
	if (ioctl(fd, XDMA_DEVICE_CONTROL, &tx_config) < 0) {
		perror("Error ioctl config tx chan");
		exit(EXIT_FAILURE);
	}
	printf("config tx chans\n");

	struct xdma_buf_info rx_buf;
	rx_buf.chan = dev.rx_chan;
	rx_buf.completion = dev.rx_cmp;
	rx_buf.cookie = (u32) NULL;
	rx_buf.buf_offset = (u32) xdma_calc_offset(dst);
	rx_buf.buf_size = (u32) xdma_calc_size(LENGTH, sizeof(dst[0]));

	rx_buf.dir = XDMA_DEV_TO_MEM;
	if (ioctl(fd, XDMA_PREP_BUF, &rx_buf) < 0) {
		perror("Error ioctl set rx buf");
		exit(EXIT_FAILURE);
	}
	printf("config rx buffer\n");

	struct xdma_buf_info tx_buf;
	tx_buf.chan = dev.tx_chan;
	tx_buf.completion = dev.tx_cmp;
	tx_buf.cookie = (u32) NULL;
	tx_buf.buf_offset = (u32) xdma_calc_offset(src);
	tx_buf.buf_size = (u32) xdma_calc_size(LENGTH, sizeof(src[0]));
	tx_buf.dir = XDMA_MEM_TO_DEV;
	if (ioctl(fd, XDMA_PREP_BUF, &tx_buf) < 0) {
		perror("Error ioctl set tx buf");
		exit(EXIT_FAILURE);
	}
	printf("config tx buffer\n");

	struct xdma_transfer rx_trans;
	rx_trans.chan = dev.rx_chan;
	rx_trans.completion = dev.rx_cmp;
	rx_trans.cookie = rx_buf.cookie;
	rx_trans.wait = 0;
	if (ioctl(fd, XDMA_START_TRANSFER, &rx_trans) < 0) {
		perror("Error ioctl start rx trans");
		exit(EXIT_FAILURE);
	}
	printf("config rx trans\n");

	struct xdma_transfer tx_trans;
	tx_trans.chan = dev.tx_chan;
	tx_trans.completion = dev.tx_cmp;
	tx_trans.cookie = tx_buf.cookie;
	tx_trans.wait = 0;
	if (ioctl(fd, XDMA_START_TRANSFER, &tx_trans) < 0) {
		perror("Error ioctl start tx trans");
		exit(EXIT_FAILURE);
	}
	printf("config tx trans\n");

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

	/* Don't forget to free the mmapped memory
	 */
	if (munmap(map, FILESIZE) == -1) {
		perror("Error un-mmapping the file");
		/* Decide here whether to close(fd) and exit() or not. Depends... */
	}

	/* Un-mmaping doesn't close the file, so we still need to do that.
	 */
	close(fd);
	return 0;
}
