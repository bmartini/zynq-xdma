/*
 * Wrapper Driver used to control a two-channel Xilinx DMA Engine
 */
#include <linux/dmaengine.h>
#include "xdma.h"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <xen/page.h>

#include <linux/slab.h>
#include <linux/amba/xilinx_dma.h>
#include <linux/platform_device.h>

static dev_t dev_num;		// Global variable for the device number
static struct cdev c_dev;	// Global variable for the character device structure
static struct class *cl;	// Global variable for the device class

char *xdma_addr;
dma_addr_t xdma_handle;

struct xdma_dev *xdma_dev_info[MAX_DEVICES + 1];
u32 num_devices;

static int xdma_open(struct inode *i, struct file *f)
{
	printk(KERN_INFO "<%s> file: open()\n", MODULE_NAME);
	return 0;
}

static int xdma_close(struct inode *i, struct file *f)
{
	printk(KERN_INFO "<%s> file: close()\n", MODULE_NAME);
	return 0;
}

static ssize_t xdma_read(struct file *f, char __user * buf, size_t
			 len, loff_t * off)
{
	printk(KERN_INFO "<%s> file: read()\n", MODULE_NAME);

	return simple_read_from_buffer(buf, len, off, xdma_addr, DMA_LENGTH);
}

static ssize_t xdma_write(struct file *f, const char __user * buf,
			  size_t len, loff_t * off)
{
	printk(KERN_INFO "<%s> file: write()\n", MODULE_NAME);
	if (len > (DMA_LENGTH - 1))
		return -EINVAL;

	copy_from_user(xdma_addr, buf, len);
	xdma_addr[len] = '\0';
	return len;
}

static int xdma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int result;
	unsigned long requested_size;
	requested_size = vma->vm_end - vma->vm_start;

	printk(KERN_INFO "<%s> file: mmap()\n", MODULE_NAME);
	printk(KERN_INFO
	       "<%s> file: memory size reserved: %d, mmap size requested: %lu\n",
	       MODULE_NAME, DMA_LENGTH, requested_size);

	if (requested_size > DMA_LENGTH) {
		printk(KERN_ERR "<%s> Error: %d reserved != %lu requested)\n",
		       MODULE_NAME, DMA_LENGTH, requested_size);

		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	result = remap_pfn_range(vma, vma->vm_start,
				 virt_to_pfn(xdma_addr),
				 requested_size, vma->vm_page_prot);

	if (result) {
		printk(KERN_ERR
		       "<%s> Error: in calling remap_pfn_range: returned %d\n",
		       MODULE_NAME, result);

		return -EAGAIN;
	}

	return 0;
}

void xdma_get_dev_info(u32 device_id, struct xdma_dev *dev)
{
	int i;

	for (i = 0; i < MAX_DEVICES; i++) {
		if (xdma_dev_info[i]->device_id == device_id)
			break;
	}
	memcpy(dev, xdma_dev_info[i], sizeof(struct xdma_dev));
}

enum dma_transfer_direction xdma_to_dma_direction(enum xdma_direction xdma_dir)
{
	enum dma_transfer_direction dma_dir;

	switch (xdma_dir) {
	case XDMA_MEM_TO_DEV:
		dma_dir = DMA_MEM_TO_DEV;
		break;
	case XDMA_DEV_TO_MEM:
		dma_dir = DMA_DEV_TO_MEM;
		break;
	default:
		dma_dir = DMA_TRANS_NONE;
		break;
	}

	return dma_dir;
}

static void xdma_sync_callback(void *completion)
{
	complete(completion);
}

void xdma_device_control(struct xdma_chan_cfg *chan_cfg)
{
	struct dma_chan *chan;
	struct dma_device *chan_dev;
	struct xilinx_dma_config config;

	config.direction = xdma_to_dma_direction(chan_cfg->dir);
	config.coalesc = chan_cfg->coalesc;
	config.delay = chan_cfg->delay;
	config.reset = chan_cfg->reset;

	chan = (struct dma_chan *)chan_cfg->chan;

	if (chan) {
		chan_dev = chan->device;
		chan_dev->device_control(chan, DMA_SLAVE_CONFIG,
					 (unsigned long)&config);
	}
}

void xdma_prep_buffer(struct xdma_buf_info *buf_info)
{
	struct dma_chan *chan;
	dma_addr_t buf;
	size_t len;
	enum dma_transfer_direction dir;
	enum dma_ctrl_flags flags;
	struct dma_async_tx_descriptor *chan_desc;
	struct completion *cmp;
	dma_cookie_t cookie;

	chan = (struct dma_chan *)buf_info->chan;
	cmp = (struct completion *)buf_info->completion;
	buf = xdma_handle + buf_info->buf_offset;
	len = buf_info->buf_size;
	dir = xdma_to_dma_direction(buf_info->dir);

	flags = DMA_CTRL_ACK | DMA_COMPL_SKIP_DEST_UNMAP | DMA_PREP_INTERRUPT;

	chan_desc = dmaengine_prep_slave_single(chan, buf, len, dir, flags);

	if (!chan_desc) {
		printk(KERN_ERR
		       "<%s> Error: dmaengine_prep_slave_single error\n",
		       MODULE_NAME);

		buf_info->cookie = -EBUSY;
	} else {
		chan_desc->callback = xdma_sync_callback;
		chan_desc->callback_param = cmp;

		// set the prepared descriptor to be executed by the engine
		cookie = chan_desc->tx_submit(chan_desc);
		if (dma_submit_error(cookie)) {
			printk(KERN_ERR "<%s> Error: tx_submit error\n",
			       MODULE_NAME);
		}

		buf_info->cookie = cookie;
	}
}

void xdma_start_transfer(struct xdma_transfer *trans)
{
	unsigned long tmo = msecs_to_jiffies(3000);
	enum dma_status status;
	struct dma_chan *chan;
	struct completion *cmp;
	dma_cookie_t cookie;

	chan = (struct dma_chan *)trans->chan;
	cmp = (struct completion *)trans->completion;
	cookie = trans->cookie;

	init_completion(cmp);
	dma_async_issue_pending(chan);

	if (trans->wait) {
		tmo = wait_for_completion_timeout(cmp, tmo);
		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
		if (0 == tmo) {
			printk(KERN_ERR "<%s> Error: transfer timed out\n",
			       MODULE_NAME);
		} else if (status != DMA_COMPLETE) {
			printk(KERN_INFO
			       "<%s> transfer: returned completion callback status of: \'%s\'\n",
			       MODULE_NAME,
			       status == DMA_ERROR ? "error" : "in progress");
		}
	}
}

void xdma_stop_transfer(struct dma_chan *chan)
{
	struct dma_device *chan_dev;

	if (chan) {
		chan_dev = chan->device;
		chan_dev->device_control(chan, DMA_TERMINATE_ALL,
					 (unsigned long)NULL);
	}
}

void xdma_test_transfer(void)
{
	const int LENGTH = 1048576;	// max image is 1024x1024 for now!

	int i;

	struct xdma_chan_cfg rx_config;
	struct xdma_chan_cfg tx_config;

	struct xdma_buf_info tx_buf;
	struct xdma_buf_info rx_buf;

	struct xdma_transfer rx_trans;
	struct xdma_transfer tx_trans;

	struct timeval ti, tf;

	memset(xdma_addr, 'Y', LENGTH);	// fill rx with a value
	xdma_addr[LENGTH - 1] = '\n';
	memset(xdma_addr + LENGTH, 'Z', LENGTH);	// fill tx with a value
	xdma_addr[LENGTH + LENGTH - 1] = '\n';

	// display contents before transfer:
	printk(KERN_INFO "<%s> test: rx buffer before transmit:\n",
	       MODULE_NAME);
	for (i = 0; i < 10; i++) {
		printk("%c\t", xdma_addr[i]);
	}
	printk("\n");

	// measure time:
	do_gettimeofday(&ti);

	rx_config.chan = xdma_dev_info[0]->rx_chan;
	rx_config.coalesc = 1;
	rx_config.delay = 0;
	xdma_device_control(&rx_config);

	tx_config.chan = xdma_dev_info[0]->tx_chan;
	tx_config.coalesc = 1;
	tx_config.delay = 0;
	xdma_device_control(&tx_config);

	rx_buf.chan = xdma_dev_info[0]->rx_chan;
	rx_buf.buf_offset = (u32) 0;
	rx_buf.buf_size = (u32) LENGTH;
	rx_buf.dir = XDMA_DEV_TO_MEM;
	rx_buf.completion = (u32) xdma_dev_info[0]->rx_cmp;
	xdma_prep_buffer(&rx_buf);

	tx_buf.chan = xdma_dev_info[0]->tx_chan;
	tx_buf.buf_offset = (u32) LENGTH;
	tx_buf.buf_size = (u32) LENGTH;
	tx_buf.dir = XDMA_MEM_TO_DEV;
	tx_buf.completion = (u32) xdma_dev_info[0]->tx_cmp;
	xdma_prep_buffer(&tx_buf);

	printk(KERN_INFO "<%s> test: xdma_start_transfer rx\n", MODULE_NAME);
	rx_trans.chan = xdma_dev_info[0]->rx_chan;
	rx_trans.wait = 0;
	rx_trans.completion = (u32) xdma_dev_info[0]->rx_cmp;
	rx_trans.cookie = rx_buf.cookie;

	printk(KERN_INFO "<%s> test: xdma_start_transfer tx\n", MODULE_NAME);
	tx_trans.chan = xdma_dev_info[0]->tx_chan;
	tx_trans.wait = 1;
	tx_trans.completion = (u32) xdma_dev_info[0]->tx_cmp;
	tx_trans.cookie = tx_buf.cookie;

	// measure time to prepare channels:
	do_gettimeofday(&tf);
	printk(KERN_INFO "<%s> test: time to prepare DMA channels [us]: %ld\n",
	       MODULE_NAME, (tf.tv_usec - ti.tv_usec));
	do_gettimeofday(&ti);	// to read transfer time only

	// start transfer:
	xdma_start_transfer(&rx_trans);
	xdma_start_transfer(&tx_trans);

	// measure time:
	do_gettimeofday(&tf);
	printk(KERN_INFO "<%s> test: DMA transfer time [us]: %ld\n",
	       MODULE_NAME, (tf.tv_usec - ti.tv_usec));
	printk(KERN_INFO "<%s> test: DMA bytes sent: %d\n", MODULE_NAME,
	       LENGTH);
	printk(KERN_INFO "<%s> test: DMA speed in Mbytes/s: %ld\n", MODULE_NAME,
	       LENGTH / (tf.tv_usec - ti.tv_usec));

	// display contents after transfer:
	printk(KERN_INFO "<%s> test: rx buffer after transmit:\n", MODULE_NAME);
	for (i = 0; i < 10; i++) {
		printk("%c\t", xdma_addr[i]);
	}
	printk("\n");
}

static long xdma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct xdma_dev xdma_dev;
	struct xdma_chan_cfg chan_cfg;
	struct xdma_buf_info buf_info;
	struct xdma_transfer trans;
	u32 devices;
	u32 chan;

	switch (cmd) {
	case XDMA_GET_NUM_DEVICES:
		printk(KERN_INFO "<%s> ioctl: XDMA_GET_NUM_DEVICES\n",
		       MODULE_NAME);

		devices = num_devices;
		if (copy_to_user((u32 *) arg, &devices, sizeof(u32)))
			return -EFAULT;

		break;
	case XDMA_GET_DEV_INFO:
		printk(KERN_INFO "<%s> ioctl: XDMA_GET_DEV_INFO\n",
		       MODULE_NAME);

		if (copy_from_user((void *)&xdma_dev,
				   (const void __user *)arg,
				   sizeof(struct xdma_dev)))
			return -EFAULT;

		xdma_get_dev_info(xdma_dev.device_id, &xdma_dev);

		if (copy_to_user((struct xdma_dev *)arg,
				 &xdma_dev, sizeof(struct xdma_dev)))
			return -EFAULT;

		break;
	case XDMA_DEVICE_CONTROL:
		printk(KERN_INFO "<%s> ioctl: XDMA_DEVICE_CONTROL\n",
		       MODULE_NAME);

		if (copy_from_user((void *)&chan_cfg,
				   (const void __user *)arg,
				   sizeof(struct xdma_chan_cfg)))
			return -EFAULT;

		xdma_device_control(&chan_cfg);
		break;
	case XDMA_PREP_BUF:
		printk(KERN_INFO "<%s> ioctl: XDMA_PREP_BUF\n", MODULE_NAME);

		if (copy_from_user((void *)&buf_info,
				   (const void __user *)arg,
				   sizeof(struct xdma_buf_info)))
			return -EFAULT;

		xdma_prep_buffer(&buf_info);

		if (copy_to_user((struct xdma_buf_info *)arg,
				 &buf_info, sizeof(struct xdma_buf_info)))
			return -EFAULT;

		break;
	case XDMA_START_TRANSFER:
		printk(KERN_INFO "<%s> ioctl: XDMA_START_TRANSFER\n",
		       MODULE_NAME);

		if (copy_from_user((void *)&trans,
				   (const void __user *)arg,
				   sizeof(struct xdma_transfer)))
			return -EFAULT;

		xdma_start_transfer(&trans);
		break;
	case XDMA_STOP_TRANSFER:
		printk(KERN_INFO "<%s> ioctl: XDMA_STOP_TRANSFER\n",
		       MODULE_NAME);

		if (copy_from_user((void *)&chan,
				   (const void __user *)arg, sizeof(u32)))
			return -EFAULT;

		xdma_stop_transfer((struct dma_chan *)chan);
		break;
	case XDMA_TEST_TRANSFER:
		printk(KERN_INFO "<%s> ioctl: XDMA_TEST_TRANSFER\n",
		       MODULE_NAME);

		xdma_test_transfer();
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = xdma_open,
	.release = xdma_close,
	.read = xdma_read,
	.write = xdma_write,
	.mmap = xdma_mmap,
	.unlocked_ioctl = xdma_ioctl,
};

static bool xdma_filter(struct dma_chan *chan, void *param)
{
	if (*((int *)chan->private) == *(int *)param)
		return true;

	return false;
}

void xdma_add_dev_info(struct dma_chan *tx_chan, struct dma_chan *rx_chan)
{
	struct completion *tx_cmp, *rx_cmp;

	tx_cmp = (struct completion *)
	    kzalloc(sizeof(struct completion), GFP_KERNEL);

	rx_cmp = (struct completion *)
	    kzalloc(sizeof(struct completion), GFP_KERNEL);

	xdma_dev_info[num_devices] = (struct xdma_dev *)
	    kzalloc(sizeof(struct xdma_dev), GFP_KERNEL);

	xdma_dev_info[num_devices]->tx_chan = (u32) tx_chan;
	xdma_dev_info[num_devices]->tx_cmp = (u32) tx_cmp;

	xdma_dev_info[num_devices]->rx_chan = (u32) rx_chan;
	xdma_dev_info[num_devices]->rx_cmp = (u32) rx_cmp;

	xdma_dev_info[num_devices]->device_id = num_devices;
	num_devices++;
}

void xdma_probe(void)
{
	dma_cap_mask_t mask;
	u32 match_tx, match_rx;
	struct dma_chan *tx_chan, *rx_chan;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE | DMA_PRIVATE, mask);

	for (;;) {
		match_tx = (DMA_MEM_TO_DEV & 0xFF) | XILINX_DMA_IP_DMA |
				(num_devices << XILINX_DMA_DEVICE_ID_SHIFT);

		tx_chan = dma_request_channel(mask, xdma_filter,
					      (void *)&match_tx);

		match_rx = (DMA_DEV_TO_MEM & 0xFF) | XILINX_DMA_IP_DMA |
				(num_devices << XILINX_DMA_DEVICE_ID_SHIFT);

		rx_chan = dma_request_channel(mask, xdma_filter,
					      (void *)&match_rx);

		if (!tx_chan && !rx_chan) {
			printk(KERN_INFO
			       "<%s> probe: number of devices found: %d\n",
			       MODULE_NAME, num_devices);
			break;
		} else {
			xdma_add_dev_info(tx_chan, rx_chan);
		}
	}
}

void xdma_remove(void)
{
	int i;

	for (i = 0; i < MAX_DEVICES; i++) {
		if (xdma_dev_info[i]) {
			if (xdma_dev_info[i]->tx_chan)
				dma_release_channel((struct dma_chan *)
						    xdma_dev_info[i]->tx_chan);

			if (xdma_dev_info[i]->tx_cmp)
				kfree((struct completion *)
				      xdma_dev_info[i]->tx_cmp);

			if (xdma_dev_info[i]->rx_chan)
				dma_release_channel((struct dma_chan *)
						    xdma_dev_info[i]->rx_chan);

			if (xdma_dev_info[i]->rx_cmp)
				kfree((struct completion *)
				      xdma_dev_info[i]->rx_cmp);

		}
	}
}

static int __init xdma_init(void)
{
	num_devices = 0;

	/* device constructor */
	printk(KERN_INFO "<%s> init: registered\n", MODULE_NAME);
	if (alloc_chrdev_region(&dev_num, 0, 1, MODULE_NAME) < 0) {
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, MODULE_NAME)) == NULL) {
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}
	if (device_create(cl, NULL, dev_num, NULL, MODULE_NAME) == NULL) {
		class_destroy(cl);
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}
	cdev_init(&c_dev, &fops);
	if (cdev_add(&c_dev, dev_num, 1) == -1) {
		device_destroy(cl, dev_num);
		class_destroy(cl);
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}

	/* allocate mmap area */
	xdma_addr =
	    dma_zalloc_coherent(NULL, DMA_LENGTH, &xdma_handle, GFP_KERNEL);

	if (!xdma_addr) {
		printk(KERN_ERR "<%s> Error: allocating dma memory failed\n",
		       MODULE_NAME);

		return -ENOMEM;
	}

	/* hardware setup */
	xdma_probe();

	return 0;
}

static void __exit xdma_exit(void)
{
	/* device destructor */
	cdev_del(&c_dev);
	device_destroy(cl, dev_num);
	class_destroy(cl);
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_INFO "<%s> exit: unregistered\n", MODULE_NAME);

	/* hardware shutdown */
	xdma_remove();

	/* free mmap area */
	if (xdma_addr) {
		dma_free_coherent(NULL, DMA_LENGTH, xdma_addr, xdma_handle);
	}
}

module_init(xdma_init);
module_exit(xdma_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Wrapper Driver For A Xilinx DMA Engine");
