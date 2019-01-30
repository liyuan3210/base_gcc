#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
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

    return 0; 
}

/*
 * �ļ��ͷź���
 * */
int mem_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * ������
 * */
static ssize_t mem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p =  *ppos;
    unsigned int count = size;
    int ret = 0;
    struct mem_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    /* ��ȡ�ź��� */
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    /*�ж϶�λ���Ƿ���Ч*/
    if (p >= MEMDEV_SIZE)
        return 0;
    if (count > MEMDEV_SIZE - p)
        count = MEMDEV_SIZE - p;

    /*�����ݵ��û��ռ�*/
    if (copy_to_user(buf, (void*)(dev->data + p), count)) 
        ret =  - EFAULT;
    else {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %d bytes(s) from %ld\n", count, p);
    }
    /* �ͷ��ź��� */
    up(&dev->sem);
    return ret;
}

/*
 * д����
 */
static ssize_t mem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
//ע�͵�����define����ִ��ifndef�������䣬����ִ�С�
//#define NOSEM
    int i, ret = size;
    volatile int j;
    char tmp[MEMDEV_SIZE];
    struct mem_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/
#ifndef NOSEM
    /* ��ȡ�ź��� */
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS; 
#endif
    memset(tmp, 0, MEMDEV_SIZE);
    if (copy_from_user(tmp, buf, size)) {
        ret = -EFAULT;
    } else {
        for (i=0; i<size; i++) {
            dev->data[dev->wpos] = tmp[i];
            if (dev->wpos < MEMDEV_SIZE-1)
                dev->wpos += 1;
            else
                dev->wpos = 0;
            msleep(1);
            //for (j=0; j<0xfffff; j++);
        }
    }
#ifndef NOSEM
    /* �ͷ��ź��� */
    up(&dev->sem);
#endif
    return ret;
}

/* 
 * seek�ļ���λ���� 
 * */
static loff_t mem_llseek(struct file *filp, loff_t offset, int whence)
{ 
    loff_t newpos;

    switch(whence) {
        case 0: /* SEEK_SET */
            newpos = offset;
            break;
        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + offset;
            break;
        case 2: /* SEEK_END */
            newpos = MEMDEV_SIZE -1 + offset;
            break;
        default: /* can't happen */
            return -EINVAL;
    }
    if ((newpos<0) || (newpos>MEMDEV_SIZE))
        return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}

/*
 * �ļ������ṹ��
 * */
static const struct file_operations mem_fops =
{
    .owner = THIS_MODULE,
    .llseek = mem_llseek,
    .read = mem_read,
    .write = mem_write,
    .open = mem_open,
    .release = mem_release,
};

/*�豸����ģ����غ���*/
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
    for (i=0; i < MEMDEV_NR_DEVS; i++) 
    {
        mem_devp[i].size = MEMDEV_SIZE;
        mem_devp[i].data = kmalloc(MEMDEV_SIZE, GFP_KERNEL);
        memset(mem_devp[i].data, 0, MEMDEV_SIZE);
        /* ��ʼ���ź��� */
        sema_init(&mem_devp[i].sem, 1);
        mem_devp[i].wpos = 0;
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
