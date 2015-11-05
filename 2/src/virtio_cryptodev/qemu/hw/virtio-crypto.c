/* 
 * virtio-crypto.c
 *
 * virtio-crypto (QEMU)
 *
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr>
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#include "iov.h"
#include "virtio.h"
#include "virtio-crypto.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/*
 * This is the struct that represents our device in the QEMU world.
 * vdev field must always be first for casting between different types.
*/
struct VirtIOCrypto {
	VirtIODevice vdev;

	/* Control VirtQueues. */
	VirtQueue *c_ivq, *c_ovq;

	/* Data VirtQueues. */
	VirtQueue *ivq, *ovq;

	/* The configuration struct of virtio crypto device. */
	struct virtio_crypto_config config;

	/* This is the file descriptor for /dev/crypto file on the Host. */
	int fd;
	
};

/*
 * Called when guest negotiates the virtio device features. 
 * We don't need any feature for virtio-crypto yet so just return 
 * the given features.
 */
static uint32_t get_features(VirtIODevice *vdev, uint32_t features)
{
	FUNC_IN;
	FUNC_OUT;
	
	return features;
}

/*
 * Called when guest requests virtio device config info. 
 */
static void get_config(VirtIODevice *vdev, uint8_t *config_data)
{
	VirtIOCrypto *crdev;
	
	FUNC_IN;
	crdev = DO_UPCAST(VirtIOCrypto, vdev, vdev);
	memcpy(config_data, &crdev->config, sizeof(struct virtio_crypto_config));
	FUNC_OUT;
}

/*
 * Called when host wants to change the virtio device config.
 * We dont need that feature so we just dummy copy config_data.
 */
static void set_config(VirtIODevice *vdev, const uint8_t *config_data)
{
	struct virtio_crypto_config config;
	
	FUNC_IN;
	memcpy(&config, config_data, sizeof(config));
	FUNC_OUT;
}

/*
 * Handler for control c_ivq virtqueue.
 * Called when host sends a control message to the guest.
 */
static void control_in(VirtIODevice *vdev, VirtQueue *vq)
{
	FUNC_IN;
	FUNC_OUT;
}

static size_t send_buffer(VirtIOCrypto *crdev, const uint8_t *buf, size_t size)
{
	VirtQueueElement elem;
	VirtQueue *vq;
	size_t len;
	size_t offset;
	
	vq = crdev->ivq;

	/* Is the virtqueue ready? */
	if (!virtio_queue_ready(vq)) {
		return 0;
	}

	
	/* kpjim: insist_send_buffer!! first push all buffers in the queue 
	 * 	  and then notify Guest */
	offset = 0;
	while (offset < size) {
	    
	    /* Is there a buffer in the queue?
	     * If not we can not send data to the Guest */
	    if (!virtqueue_pop(vq, &elem)) {
		break;
	    }
	    
	    len = iov_from_buf(elem.in_sg, elem.in_num, 0,
			       buf + offset, size - offset);
	    offset += len;
	    
	    /* Push the buffer to the virtqueue... */
	    virtqueue_push(vq, &elem, len);
	}

	/* ...and notify guest */
	virtio_notify(&crdev->vdev, vq);
// 	/* kpjim */

	return len;
}

/*
 * Send a control event to the guest.
 */
static size_t send_control_event(VirtIOCrypto *crdev, uint16_t event, 
                                 uint16_t value)
{
	struct virtio_crypto_control cpkt;
	VirtQueueElement elem;
	VirtQueue *vq;

	FUNC_IN;
	stw_p(&cpkt.event, event);
	stw_p(&cpkt.value, value);

	vq = crdev->c_ivq;
	if (!virtio_queue_ready(vq)) {
		return 0;
	}

	if (!virtqueue_pop(vq, &elem)) {
		return 0;
	}
	
	memcpy(elem.in_sg[0].iov_base, &cpkt, sizeof(cpkt));
	
	virtqueue_push(vq, &elem, sizeof(cpkt));
	virtio_notify(&crdev->vdev, vq);
	FUNC_OUT;
	return sizeof(cpkt);
}

/*
 * This is the function that is called whenever the guest sends us 
 * control events.
 */
/* kpjim: tha grafei sto value.. -1 gia error, fd gia success */
static void handle_control_message(VirtIOCrypto *crdev, void *buf, size_t len)
{
	struct virtio_crypto_control cpkt, *gcpkt;

	FUNC_IN;
	gcpkt = buf;

	if (len < sizeof(cpkt)) {
		/* The guest sent an invalid control packet */
		return;
	}

	cpkt.event = lduw_p(&gcpkt->event);
	cpkt.value = lduw_p(&gcpkt->value);

	if (cpkt.event == VIRTIO_CRYPTO_DEVICE_GUEST_OPEN) {
		/* cpkt.value = 1 for file open and 0 for file close. */
		if (cpkt.value) {
			printf("in open file\n");

			/* Open crypto device file and send the appropriate
 			 * message (event) to the guest */
			/* ? */
			
			/* kpjim */
			
			if ((crdev->fd = open("/dev/crypto", O_RDWR)) < 0)
			    gcpkt->value = -1;
			else
			    gcpkt->value = crdev->fd;	    
		} 
		else {
			printf("in close file\n");

			/* Close the previously opened file */
			/* ? */
			/* kpjim */
			
			if (close(crdev->fd) < 0)
			    gcpkt->value = -1;
			else {
			    gcpkt->value = -13;
			    crdev->fd = -13;
			}
		}
		send_control_event(crdev, VIRTIO_CRYPTO_DEVICE_HOST_OPEN, gcpkt->value);
		/* kpjim */
	}
	FUNC_OUT;
}

/*
 * Handler for control c_ovq virtqueue.
 * Called when guest sends a control message to the host.
 */
static void control_out(VirtIODevice *vdev, VirtQueue *vq)
{
	VirtQueueElement elem;
	VirtIOCrypto *crdev;
	uint8_t *buf;
	size_t len;

	FUNC_IN;	
	crdev = DO_UPCAST(VirtIOCrypto, vdev, vdev);
	
	len = 0;
	buf = NULL;

	if (!virtqueue_pop(vq, &elem))
		return;

	len = iov_size(elem.out_sg, elem.out_num);
	buf = g_malloc(len);
	
	iov_to_buf(elem.out_sg, elem.out_num, 0, buf, len);
	handle_control_message(crdev, buf, len);

	virtqueue_push(vq, &elem, 0);
	virtio_notify(vdev, vq);

	g_free(buf);
	FUNC_OUT;
}

/*
 * Handler for ivq virtqueue.
 * Called when host sends data to the guest.
 * No need to worry about it.
 */
static void handle_input(VirtIODevice *vdev, VirtQueue *vq)
{
	FUNC_IN;
	FUNC_OUT;
}

/*
 * Performs ioctl to the actual device
 */
static ssize_t crypto_handle_ioctl_packet(VirtIOCrypto *crdev, 
                                          const uint8_t *buf, 
                                          ssize_t len) 
{
	ssize_t ret = 0;
	struct crypt_op crypt;
	struct session_op sess;
	virtio_crypto_buffer *cr_data = (virtio_crypto_buffer *) buf;
	uint32_t sess_id;
	
	FUNC_IN;
	switch (cr_data->cmd) {

	/* set the metadata for every operation and perform the ioctl to 
 	 * the actual device */
	case CIOCGSESSION:
		/* ? */
		
		memcpy(&sess, &cr_data->op.sess, sizeof(struct session_op));
		sess.key = cr_data->keyp;
		
		if ( ioctl(crdev->fd, CIOCGSESSION, &sess) < 0){
		    perror("ioctl(CIOCGSESSION)");
		    /* ... */
		}
		
		sess.key = cr_data->op.sess.key;
		memcpy(&cr_data->op.sess, &sess, sizeof(struct session_op));
		printf("ioctl(CIOCGSESSION): Sent SessID: %d\n", cr_data->op.sess.ses);
		
		break;

	case CIOCCRYPT:
		/* ? */
		/* copy data to crypt struct */
		memcpy(&crypt, &cr_data->op.crypt, sizeof(struct crypt_op));
		
		crypt.len = sizeof(cr_data->srcp);
		crypt.src = cr_data->srcp;
		crypt.dst = cr_data->dstp;
		crypt.iv  = cr_data->ivp;
		
		/* debug */
		printf("\nioctl(CIOCCRYPT):\nSessID: %d\n", crypt.ses);
		printf("source is: %s\n", crypt.src);
		if (crypt.op == COP_ENCRYPT)
		    printf("operation: COP_ENCRYPT\n\n");
		if (crypt.op == COP_DECRYPT)
		    printf("operation: COP_DECRYPT\n\n");
		
		/* perform ioctl */
		if (ioctl(crdev->fd, CIOCCRYPT, &crypt) < 0){
		    perror("ioctl(CIOCCRYPT)");
		    /* ... */
		}
		
		break;

	case CIOCFSESSION:
		/* ? */
		/* kpjim: den eimai sigouros pws xreiazetai auto.. */
		memcpy(&sess_id, &cr_data->op.sess_id, sizeof(uint32_t));
		
		if (ioctl(crdev->fd, CIOCFSESSION, &sess_id) < 0){
		    perror("ioctl(CIOCFSESSION)");
// 		    ret = -EAGAIN; 
		    /* ... */
		}
		
		break;

	default:
		return -EINVAL;
	}

	/* Ok now we can send the reply to the guest */
	/* ? */
	/* kpjim: ypothetw edw kanoume push sto ivq?? 
	 * 	  'h send_buffer? */
	ret = send_buffer(crdev, (const uint8_t *)cr_data, sizeof(virtio_crypto_buffer));


	FUNC_OUT;
	return ret;
}

/*
 * Handler for ovq virtqueue.
 * Called when guest sends data to the host.
 */
static void handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
	VirtIOCrypto *crdev = DO_UPCAST(VirtIOCrypto, vdev, vdev);
	VirtQueueElement elem;
	uint8_t *buf;
	size_t buf_size;
	ssize_t ret;

	FUNC_IN;
	
	/* Dummy return. Delete once the driver is ready. */
// 	return; 

	/* pop buffer from virtqueue and return value */
	/* ? */
	/* kpjim: if (!pop()) return 0; ??? */
	if (!virtqueue_pop(vq, &elem))
	    return;
	

	/* FIXME: Are we sure all data is in one sg list?? */
	buf = elem.out_sg[0].iov_base;
	buf_size = elem.out_sg[0].iov_len;
	ret = crypto_handle_ioctl_packet(crdev, buf, buf_size);

	/* Put buffer back and notify guest. */
	/* ? */
	/* kpjim: enw edw kanoume push sto ovq */
	virtqueue_push(vq, &elem, 0);
	virtio_notify(vdev, vq);

	FUNC_OUT;
}

/*
 * Called from virtio_crypto_init_pci() before the initialization of
 * the pci device.
 *
 * We need to create the virtio device, add the needed virtqueues and
 * set all the handlers we need.
 */
VirtIODevice *virtio_crypto_init(DeviceState *dev)
{
	VirtIOCrypto *crdev;
	VirtIODevice *vdev;

	FUNC_IN;
	vdev = virtio_common_init("virtio-crypto", VIRTIO_ID_CRYPTO,
	                          sizeof(struct virtio_crypto_config),
	                          sizeof(VirtIOCrypto));

	crdev = DO_UPCAST(VirtIOCrypto, vdev, vdev);

	/* Add the virtqueues we need for the device. */
	crdev->c_ivq = virtio_add_queue(vdev, 32, control_in);
	crdev->c_ovq = virtio_add_queue(vdev, 32, control_out);
	crdev->ivq = virtio_add_queue(vdev, 128, handle_input);
	crdev->ovq = virtio_add_queue(vdev, 128, handle_output);

	/* An initial value to indicate not opened file. */
	crdev->fd = -13;

	/* Set the callback functions for the virtio device we made. */
	crdev->vdev.get_features = get_features;
	crdev->vdev.get_config = get_config;
	crdev->vdev.set_config = set_config;

	FUNC_OUT;
	return vdev;
}

/*
 * Called from virtio_crypto_exit_pci() 
 */
void virtio_crypto_exit(VirtIODevice *vdev)
{
	FUNC_IN;
	FUNC_OUT;
}
