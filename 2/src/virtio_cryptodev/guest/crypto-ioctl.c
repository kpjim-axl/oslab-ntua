/*
 * crypto-ioctl.c
 *
 * Implementation of ioctl for 
 * virtio-crypto (guest side) 
 *
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr>
 *                                                                               
 */

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/ioctl.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include "crypto.h"
#include "crypto-vq.h"
#include "crypto-chrdev.h"

void insist_read_buf(struct crypto_device *crdev, struct crypto_data *cr_data, ssize_t size);

void insist_read_buf(struct crypto_device *crdev, struct crypto_data *cr_data, ssize_t size){
    ssize_t ret = 0;
    
    while (ret < size)
	ret += fill_readbuf(crdev, (char *)cr_data, sizeof(struct crypto_data));
    
    return;
}

long crypto_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct crypto_device *crdev;
	struct crypto_data *cr_data;
	ssize_t ret;

	crdev = filp->private_data;

	cr_data = kzalloc(sizeof(struct crypto_data), GFP_KERNEL);
	if (!cr_data)
		return -ENOMEM;

	cr_data->cmd = cmd;

	switch (cmd) {

        /* get the metadata for every operation 
	 * from userspace and send the buffer 
         * to the host */

	/* kpjim: structs session_op and crypt_op, contain pointers. We need to track that data and copy them as well. */
	/* FIXME kpjim: memcpy ta kleidia klp. Otan staloun ta dedomena, tha staloun mono oi dieuthunseis twn kleidiwn
	 * 		opote prepei na antigrapseis oloklhres tis domes */
	case CIOCGSESSION:
		/* ? */
		
		/* kpjim: get session_op from userspace, and send it to the host */
		ret = copy_from_user((void *) &cr_data->op.sess, (void __user *) arg, sizeof(struct session_op));
		
		/* kpjim: pros8hkh twn kleidiwn... Mporei na thelei 3ana copy_from_user 
		 * 	  thymisou meta na alla3eis kai to struct session_op na deixnei ekei */
		ret = copy_from_user((void *) cr_data->keyp, (void __user *) cr_data->op.sess.key, CRYPTO_CIPHER_MAX_KEY_LEN);
		
		/* kpjim: send the buffer to hyper */
		ret = send_buf(crdev, (void *) cr_data, sizeof(*cr_data), (filp->f_flags & O_NONBLOCK));
		
		/* kpjim */

		if (!device_has_data(crdev)) {
			debug("sleeping in CIOCGSESSION\n");
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;

			/* Go to sleep until we have data. */
			ret = wait_event_interruptible(crdev->i_wq,
			                               device_has_data(crdev));

			if (ret < 0) 
				goto free_buf;
		}
		printk(KERN_ALERT "woke up in CIOCGSESSION\n");
		

		if (!device_has_data(crdev))
			return -ENOTTY;

		insist_read_buf(crdev, cr_data, sizeof(struct crypto_data));

		debug("ioctl(CIOCGSESSION): Got SessID: %d\n", cr_data->op.sess.ses);
		/* copy the response to userspace */
		/* ? */
		/* kpjim: copy struct session_op back to userspace.. No need to copy the key again.. I think! */
		ret = copy_to_user((void __user *) arg, (const void *) &cr_data->op.sess, sizeof(struct session_op));
		if (ret)
		    goto free_buf;
		/* kpjim */

		break;

	case CIOCCRYPT: 
		/* ? */
		
		/* kpjim: get crypt_op from userspace and send it to the host */
		ret = copy_from_user((void *) &cr_data->op.crypt, (void __user *) arg, sizeof(struct crypt_op));
		
		/* kpjim: pros8hkh extra dedomenwn gia apofugh address errors: crypt->src, crypt->dst, crypt->iv
		 * 	  hmm.. twra pou to skeftomai.. mallon dn xreiazetai auto twra.. mallon prepei na ananewsw to struct
		 * 	  wste na deixnei sta kainouria buffers kai meta na antigrapsw ta buffers sth thesh twn arxikwn dst kai src
		 */
		ret = copy_from_user((void *) cr_data->srcp, (void __user *) cr_data->op.crypt.src, CRYPTO_DATA_MAX_LEN);
		ret = copy_from_user((void *) cr_data->dstp, (void __user *) cr_data->op.crypt.dst, CRYPTO_DATA_MAX_LEN);
		ret = copy_from_user((void *) cr_data->ivp,  (void __user *) cr_data->op.crypt.iv,  CRYPTO_BLOCK_MAX_LEN);
		debug("ioctl(CIOCCRYPT): SessID: %d\n", cr_data->op.crypt.ses);
// 		debug("source: %s\n", cr_data->srcp);
		
		/* kpjim: send the buffer to hyper */
		ret = send_buf(crdev, (void *) cr_data, sizeof(*cr_data), (filp->f_flags & O_NONBLOCK));
		/* kpjim */

		if (!device_has_data(crdev)) {
			printk(KERN_WARNING "sleeping in CIOCCRYPTO\n");	
			if (filp->f_flags & O_NONBLOCK)
			    return -EAGAIN;
			
			/* Go to sleep until we have data. */
			ret = wait_event_interruptible(crdev->i_wq,
			device_has_data(crdev));
			
			if (ret < 0)
			    goto free_buf;
		}

		insist_read_buf(crdev, cr_data, sizeof(struct crypto_data));
		ret = copy_to_user((void __user *)arg, &cr_data->op.crypt, 
		                   sizeof(struct crypt_op));
// 		if (ret)
// 			goto free_buf;

		/* copy the response to userspace */
		/* kpjim: we've already copied the struct crypt_op, but we need to update src(?) and dest data.
		 * 	  FIXME: make sure that we've changed back crypt->op.crypt->dst buffer's address..
		 * update: to src kai to iv den allazoun.. opote den yparxei logos na ta antigrapsoume pisw..
		 */
		debug("ioctl(CIOCCRYPT): returning SessID: %d\n", cr_data->op.crypt.ses);
		
		ret = copy_to_user((void __user *) cr_data->op.crypt.dst, cr_data->dstp, cr_data->op.crypt.len);
		/* ? */

		break;

	case CIOCFSESSION:

		/* ? */
		/* kpjim: get sess_id from userspace and send it to the host */
		ret = copy_from_user((void *) &cr_data->op.sess_id, (void __user *) arg, sizeof(uint32_t));
		ret = send_buf(crdev, (void *) cr_data, sizeof(*cr_data), (filp->f_flags & O_NONBLOCK));
		/* 'h ret = get_user(cr_data->op.sess_id, (uint32_t __user *)arg); */
		/* kpjim */
		
		if (!device_has_data(crdev)) {
			printk(KERN_WARNING "PORT HAS NOT DATA!!!\n");
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;

			/* Go to sleep until we have data. */
			ret = wait_event_interruptible(crdev->i_wq,
			                               device_has_data(crdev));

			if (ret < 0)
				goto free_buf;
		}

		insist_read_buf(crdev, cr_data, sizeof(struct crypto_data));

		/* copy the response to userspace */
		/* ? */
		/* kpjim */
		ret = copy_to_user((void __user *) arg, (const void *) &cr_data->op.sess_id, sizeof(uint32_t));
		/* kpjim */

		break;

	default:
		return -EINVAL;
	}

	return 0;	// looks suspicious...

free_buf:
	kfree(cr_data);
	return ret;	
}
