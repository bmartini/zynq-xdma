// the below define is a hack
#define u32 unsigned int
#define dma_cookie_t unsigned int
#include "xdma.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define FILEPATH "/dev/xdma"
#define NUMINTS  (1000)
#define FILESIZE (NUMINTS * sizeof(int))

int main(int argc, char *argv[])
{
	int fd;

	/* Open a file for writing.
	 */
	fd = open(FILEPATH, O_RDWR, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}

	/* Query driver to run test function.
	 */
	if (ioctl(fd, XDMA_TEST_TRANSFER, NULL) < 0) {
		perror("Error ioctl test transition");
		exit(EXIT_FAILURE);
	}
	printf("ran driver test transition: use 'dmesg' to view results\n");

	close(fd);

	return 0;
}
