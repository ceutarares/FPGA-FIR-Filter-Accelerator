#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>       // for file_operations
#include <linux/cdev.h>     // for struct cdev
#include <linux/uaccess.h>  // for copy_to_user / copy_from_user
#include <linux/spi/spi.h>  
#include <linux/device.h>
#include <linux/ioctl.h>


#define DRIVER_NAME "fpga_fir"
#define CLASS_NAME  "fpga_class"


struct fpga_dev {
    struct spi_device *spi; 
    struct cdev cdev; 
    int major_number; 
    struct class *fpga_class;
    struct device *fpga_device;  

};

struct fpga_spi_ioc_transfer{
    __u64 tx_buf;
    __u64 rx_buf;
    __u32 len;
};

#define FPGA_IOC_MAGIC 'k'
#define FPGA_IOC_FULL_DUPLEX _IOWR(FPGA_IOC_MAGIC, 1, struct fpga_spi_ioc_transfer)

//global variable for our device
static struct fpga_dev my_fpga;

static int fpga_open(struct inode *inode, struct file *file){
    pr_info("%s: Device open.\n", DRIVER_NAME);
    return 0;
}

static int fpga_release(struct inode *inode, struct file *file){
    pr_info("%s: Device closed.\n", DRIVER_NAME);
    return 0;
}


static long fpga_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct fpga_spi_ioc_transfer user_data;
    u8 *tx_buf = NULL, *rx_buf = NULL;
    int ret = 0;
    struct spi_transfer t = {0};
    struct spi_message m;

    
    if (cmd != FPGA_IOC_FULL_DUPLEX) {
        return -ENOTTY;
    }

    
    if (copy_from_user(&user_data, (void __user *)arg, sizeof(user_data))) {
        return -EFAULT;
    }

    
    tx_buf = kmalloc(user_data.len, GFP_KERNEL);
    rx_buf = kzalloc(user_data.len, GFP_KERNEL);
    if (!tx_buf || !rx_buf) {
        ret = -ENOMEM;
        goto out_free; 
    }

    
    if (copy_from_user(tx_buf, (u8 __user *)(uintptr_t)user_data.tx_buf, user_data.len)) {
        ret = -EFAULT;
        goto out_free;
    }

    
    t.tx_buf = tx_buf;
    t.rx_buf = rx_buf;
    t.len = user_data.len;
    
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    ret = spi_sync(my_fpga.spi, &m); 
    if (ret < 0) {
        pr_err("%s: Eroare la transferul SPI: %d\n", DRIVER_NAME, ret);
        goto out_free;
    }

    
    if (copy_to_user((u8 __user *)(uintptr_t)user_data.rx_buf, rx_buf, user_data.len)) {
        ret = -EFAULT;
    }

out_free:
    kfree(tx_buf);
    kfree(rx_buf);
    
    return ret;
}



//file_operation map our functions to system functions
//when the user-space program will call open(..), our 
//driver will know to call fpga_open.  
static struct file_operations fpga_fops = {
    .owner = THIS_MODULE, 
    .open = fpga_open, 
    .release = fpga_release, 
    .unlocked_ioctl = fpga_ioctl, 
};

static int fpga_probe(struct spi_device *spi){
    int ret; 
    pr_info("%s: FPGA detected on bus.\n", DRIVER_NAME);

    //save the spi data after detecting fpga on bus
    my_fpga.spi = spi; 

    //alocate major number for our driver
    ret = alloc_chrdev_region(&my_fpga.major_number, 0, 1, DRIVER_NAME);
    if (ret < 0) return ret;

    //init and add cdev structure in kernel 
    cdev_init(&my_fpga.cdev, &fpga_fops);
    ret = cdev_add(&my_fpga.cdev, my_fpga.major_number, 1);
    if (ret < 0) goto unregister_chrdev;

    my_fpga.fpga_class = class_create(CLASS_NAME);
    my_fpga.fpga_device = device_create(my_fpga.fpga_class, NULL, my_fpga.major_number, NULL, DRIVER_NAME);

    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8; 
    spi_setup(spi);

    return 0;

    unregister_chrdev:
    unregister_chrdev_region(my_fpga.major_number, 1);
    return ret; 
}

//free resources 
static void fpga_remove(struct spi_device *spi){
    pr_info("%s: FPGA disconnected.\n", DRIVER_NAME);
    device_destroy(my_fpga.fpga_class, my_fpga.major_number);
    class_destroy(my_fpga.fpga_class);
    cdev_del(&my_fpga.cdev);
    unregister_chrdev_region(my_fpga.major_number, 1);
}

static const struct of_device_id fpga_dt_ids[] = {
    {.compatible = "custom,fpga-fir"},
    { }
};

//to detect spi on bus 
static const struct spi_device_id fpga_ids[] = {
    { "fpga-fir", 0 },
    { }
};

MODULE_DEVICE_TABLE(spi, fpga_ids);



MODULE_DEVICE_TABLE(of, fpga_dt_ids);

static struct spi_driver fpga_spi_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = fpga_dt_ids,
    },
    .probe = fpga_probe,
    .remove = fpga_remove,
    .id_table = fpga_ids, 
};

module_spi_driver(fpga_spi_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ceuta Rares");
MODULE_DESCRIPTION("Driver SPI for FPGA FIR-Filter");
MODULE_VERSION("1.0");