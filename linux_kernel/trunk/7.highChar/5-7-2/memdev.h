
#ifndef _MEMDEV_H_
#define _MEMDEV_H_

#ifndef MEMDEV_MAJOR
#define MEMDEV_MAJOR 251   /**/
#endif

#ifndef MEMDEV_NR_DEVS
#define MEMDEV_NR_DEVS 2    /**/
#endif

#ifndef MEMDEV_SIZE
#define MEMDEV_SIZE 4096
#endif

/*mem*/
struct mem_dev                                     
{     
    bool canRead;   /*�豸�ɶ���ʶ*/
    bool canWrite;  /*�豸��д��ʶ*/                                                   
    char *data;                      
    unsigned long size; 
    unsigned long rpos; /*����λ��ʶ*/
    unsigned long wpos; /*д��λ��ʶ*/
    unsigned long nattch;
    wait_queue_head_t rwq;
    wait_queue_head_t wwq;      
    struct semaphore sem; /*��ռʽ�ں�ʱ��Ҫ���*/ 
};

#endif /* _MEMDEV_H_ */
