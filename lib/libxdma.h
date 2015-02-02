#ifndef LIBXDMA_H
#define LIBXDMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FILEPATH "/dev/xdma"
#define MAP_SIZE  (33554432)
#define FILESIZE (MAP_SIZE * sizeof(uint8_t))

	enum xdma_wait {
		XDMA_WAIT_NONE = 0,
		XDMA_WAIT_SRC = (1 << 0),
		XDMA_WAIT_DST = (1 << 1),
		XDMA_WAIT_BOTH = (1 << 1) | (1 << 0),
	};

	void *xdma_alloc(int length, int byte_num);

	void xdma_alloc_reset(void);

	int xdma_init(void);

	int xdma_exit(void);

	int xdma_num_of_devices(void);

	int xdma_perform_transaction(int device_id, enum xdma_wait wait,
				     uint32_t * src_ptr, uint32_t src_length,
				     uint32_t * dst_ptr, uint32_t dst_length);

	int xdma_stop_transaction(int device_id,
				  uint32_t * src_ptr, uint32_t src_length,
				  uint32_t * dst_ptr, uint32_t dst_length);

#ifdef __cplusplus
}
#endif
#endif				/* LIBXDMA_H */
