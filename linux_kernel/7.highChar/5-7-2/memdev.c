#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include "memdev.h"

static int mem_major = MEMDEV_MAJOR;
module_param(mem_major, int, S_IRUGO);

struct mem_dev *mem_devp; /*�豸�ṹ��ָ��*/
struct cdev cdev; 

/*
 * �ļ��򿪺���
 * */
int mem_open(struct inode *inode, struct file *filp)
{
    struct mem_dev *dev;
    /*��ȡ���豸��*/
    int num = MINOR(inode->i_rdev);
    if (num >= MEMDEV_NR_DEVS) 
        return -ENODEV;
    dev = &mem_devp[num];
    /*���豸�����ṹָ�븳ֵ���ļ�˽������ָ��*/
    filp->private_data = dev;
    dev->nattch++;
    return 0; 
}

/*
 * �ļ��ͷź���
 * */
int mem_release(struct inode *inode, struct file *filp)
{
    struct mem_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/
    dev->nattch--;
    if (0==dev->nattch) {
        /*ʹ��дλ�ñ�־����ָ����ͷ, ���޸Ŀɶ���д��־*/
        dev->rpos = 0;
        dev->wpos = 0;
        dev->canRead  = false; 
        dev->canWrite = true;  
        /*printk("<1> [release and renew dev]");*/
    }
    return 0;
}

/*
 * ������
 * */
static ssize_t mem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int count;
    int ret = 0;
    struct mem_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    /*printk("<1> [Before mem_read: rpos=%lu, wpos=%lu, canR=%d, canW=%d]\n", 
            dev->rpos, dev->wpos, dev->canRead, dev->canWrite);*/
    /* ����ʹ��while�ĺô��ǿ��Ա�֤����Ϊ�����ݿɶ��������ģ���
     * ʹ��while�ִ���������һ�����⣺�޷�ͨ���ж��ź�����ѭ��*/
    if (!dev->canRead) {
        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        wait_event_interruptible(dev->rwq, dev->canRead);
    }
    if(down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (dev->rpos < dev->wpos) {
        count = dev->wpos - dev->rpos;
        count = count>size ? size : count;
    } else {
        count = MEMDEV_SIZE - dev->rpos - 1;
        if (count >= size) {
            count = size;    
        } else {
            if (copy_to_user(buf, (void*)(dev->data + dev->rpos), count)) 
                ret =  - EFAULT;
            ret += count;
            dev->rpos = 0;
            buf  += count;
            size -= count;
            count = dev->wpos>size ? size: dev->wpos;
        }
    }
    /*�����ݵ��û��ռ�*/
    if (copy_to_user(buf, (void*)(dev->data + dev->rpos), count)) {
        ret =  - EFAULT;
    } else {
        dev->rpos += count;
        ret += count;
    }
    
    if (ret) {
        dev->canWrite = true;        /*�пռ��д*/
        wake_up(&(dev->wwq));   /*����д�ȴ�����*/
        if (dev->rpos==dev->wpos)
            dev->canRead = false;    /*�����ݿɶ�*/
    }
    /*printk("<1> [After mem_read: rpos=%lu, wpos=%lu, canR=%d, canW=%d]\n", 
            dev->rpos, dev->wpos, dev->canRead, dev->canWrite);*/
    up(&dev->sem);    
    return ret;
}

/*
 * д����
 * */
static ssize_t mem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int count;
    int ret = 0;
    struct mem_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    /*printk("<1> [Before mem_write: rpos=%lu, wpos=%lu, canR=%d, canW=%d]\n", 
            dev->rpos, dev->wpos, dev->canRead, dev->canWrite);*/

    /* û�пռ��д�������˯�� */
    if (!dev->canWrite) {
        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        wait_event_interruptible(dev->wwq, dev->canWrite);
    }
    if(down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (dev->rpos > dev->wpos) {
        count = dev->rpos - dev->wpos;
        count = count>size ? size : count;
    } else {
        count = MEMDEV_SIZE - dev->wpos - 1;
        if (count >= size) {
            count = size;    
        } else {
            if (copy_from_user(dev->data + dev->wpos, buf, count)) 
                ret =  - EFAULT;
            ret += count;
            dev->wpos = 0;
            buf  += count;
            size -= count;
            count = dev->rpos>size ? size: dev->rpos;
        }
    }

    /*���û��ռ�д������*/
    if (copy_from_user(dev->data + dev->wpos, buf, count)) {
        ret =  - EFAULT;
    } else {
        dev->wpos += count;
        ret += count;
    }
    
    if (ret) {
        dev->canRead = true;         /*�пռ��д*/
        wake_up(&(dev->rwq));   /*���Ѷ��ȴ�����*/
        if (dev->rpos==dev->wpos)
            dev->canWrite = false;    /*�����ݿ�д*/
    }
    /*printk("<1> [After mem_write: rpos=%lu, wpos=%lu, canR=%d, canW=%d]\n", 
            dev->rpos, dev->wpos, dev->canRead, dev->canWrite);*/
    up(&dev->sem);    
    return ret;
}

/*
 * �ļ������ṹ��
 * */
static const struct file_operations mem_fops =
{
    .owner = THIS_MODULE,
    .read = mem_read,
    .write = mem_write,
    .open = mem_open,
    .release = mem_release,
};

/*
 * �豸����ģ����غ���
 * */
static int memdev_init(void)
{
    int result;
    int i;

    dev_t devno = MKDEV(mem_major, 0);

    /* ��̬�����豸��*/
    if (mem_major)
        result = register_chrdev_region(devno, 2, "memdev");
    else { /* ��̬�����豸�� */
        result = alloc_chrdev_region(&devno, 0, 2, "memdev");
        mem_major = MAJOR(devno);
    }  

    if (result < 0)
        return result;

    /*��ʼ��cdev�ṹ*/
    cdev_init(&cdev, &mem_fops);
    cdev.owner = THIS_MODULE;
    cdev.ops = &mem_fops;

    /* ע���ַ��豸 */
    cdev_add(&cdev, MKDEV(mem_major, 0), MEMDEV_NR_DEVS);

    /* Ϊ�豸�����ṹ�����ڴ�*/
    mem_devp = kmalloc(MEMDEV_NR_DEVS * sizeof(struct mem_dev), GFP_KERNEL);
    if (!mem_devp)    /*����ʧ��*/
    {
        result =  - ENOMEM;
        goto fail_malloc;
    }
    memset(mem_devp, 0, sizeof(struct mem_dev));

    /*Ϊ�豸�����ڴ�*/
    for (i=0; i < MEMDEV_NR_DEVS; i++)  {
        mem_devp[i].size = MEMDEV_SIZE;
        mem_devp[i].data = kmalloc(MEMDEV_SIZE, GFP_KERNEL);
        memset(mem_devp[i].data, 0, MEMDEV_SIZE);

        mem_devp[i].canRead  = false; /*һ��ʼ�豸û�����ݿɹ���*/
        mem_devp[i].canWrite = true;  /*һ��ʼ�豸�пռ�ɹ�д*/
        /*��ʼ����дָ��*/
        mem_devp[i].rpos = 0;
        mem_devp[i].wpos = 0;
        /*��ʼ���ȴ�����*/
        init_waitqueue_head(&(mem_devp[i].rwq));
        init_waitqueue_head(&(mem_devp[i].wwq));
        /*�豸�ļ����򿪵Ĵ���*/
        mem_devp[i].nattch = 0;
        /*��ʼ���ź���*/
        sema_init(&mem_devp[i].sem, 1);
    }
    return 0;
fail_malloc: 
    unregister_chrdev_region(devno, 1);
    return result;
}

/*ģ��ж�غ���*/
static void memdev_exit(void)
{
    cdev_del(&cdev);   /*ע���豸*/
    kfree(mem_devp);     /*�ͷ��豸�ṹ���ڴ�*/
    unregister_chrdev_region(MKDEV(mem_major, 0), 2); /*�ͷ��豸��*/
}

MODULE_AUTHOR("www.enjoylinux.cn");
MODULE_LICENSE("GPL");

module_init(memdev_init);
module_exit(memdev_exit);
