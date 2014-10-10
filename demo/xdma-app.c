#include "libxdma.h"

#include <stdio.h>
#include <stdlib.h>

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
		printf("%c\t", dst[i]);
	}
	printf("\n");

	if (0 < xdma_num_of_devices()) {
		xdma_perform_transaction(0, XDMA_WAIT_NONE, src, LENGTH, dst,
					 LENGTH);
	}

	printf("test: dst buffer after transmit:\n");
	for (i = 0; i < 10; i++) {
		printf("%c\t", dst[i]);
	}
	printf("\n");

	xdma_exit();

	return 0;
}
