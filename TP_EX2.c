/* Pour include les sources kernel linux sur le raspi :
    $ sudo apt-get install linux-headers-rpi-rpfv
    puis 
    $ sudo apt-get install linux-images-rpi-rpfv 
    reboot le rpi et normalement ça devrait marcher
    $ sudo rpi-update
    $ sudo apt-get install raspberrypi-kernel-headers => essayer celui la en premier, peut etre qu'on a besoin que de lui
    $ sudo apt-get install raspberrypi-kernel */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

#define LICENCE "GPL"
#define AUTEUR "Emmanuel CAUSSE emmanuel.causse@univ-tlse3.fr"
#define DESCRIPTION "Module GPIO pour KY-050"
#define DEVICE "camsi3"
#define BUFFER_SIZE 1024
#define MIN( p1, p2 ) ( p1 <= p2 ) ? p1 : p2

#define PIN_SIGNAL 18
#define HIGH 1
#define LOW 0

dev_t dev;

static ssize_t buffer_read(struct file *f, char *buff, size_t size, loff_t *offp);
static ssize_t buffer_write(struct file *f, const char * buff, size_t size, loff_t *offp);
static int buffer_open(struct inode *in, struct file *f);
static int buffer_release(struct inode *in, struct file *f);

struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.read = &buffer_read,
	.write = &buffer_write,
	.open = &buffer_open,
	.release = &buffer_release
};

struct cdev *my_cdev;

static int buffer_init(void){
	
	int err;

	// Allocationn dynamique pour les paires majeur/mineur
	if(alloc_chrdev_region(&dev,0,1,"ky004") == -1){
		printk(KERN_ALERT "Erreur alloc_chrdev_region\n");
		return -EINVAL;
	}

	printk(KERN_ALERT "Init alloc (majeur,mineur)=(%d,%d)\n",MAJOR(dev),MINOR(dev));
	
	//Enregistrement du peripherique
	my_cdev = cdev_alloc();
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	err=cdev_add(my_cdev,dev,1);

	if(err){printk(KERN_ALERT "Erreur cdev_add \n");}
	
	//Configuration des broche d'entrées/sorties
	//La broche signal est à l'état haut (résistance de pull-up)
    gpio_request_one(PIN_SIGNAL,GPIOF_OUT_INIT_HIGH,"signal");

	return 0;
}

long signal_recu = 0;
long signal_recu_max = 1;

// fonction appeler lorsqu'ont fait le 'cat'
static ssize_t buffer_read(struct file *f, char *buff, size_t size, loff_t *offp){
    printk(KERN_ALERT "Read called!\n");
    signal_recu = 0;
	while(1){
		while(gpio_get_value(PIN_SIGNAL) != 0){
			 udelay(100);
		}
		signal_recu++;
		printk("Signal recu %ld fois\n",signal_recu);
		if(signal_recu == signal_recu_max){
			//Nombre de donné de signaux recu, attente terminée	
			return 0;
		}
		udelay(100);
	}
}

//Fonction appelé lorsqu'on fait un echo "blalbla" > /dev/mondriver
static ssize_t buffer_write(struct file *f, const char * buff, size_t size, loff_t *offp){
	//printk(KERN_ALERT "Write not implemented\n"); // version 1
	
	//Version 2
	char buffData[10];
	//Copie de la valeur a écrire dans /dev/ky004
	copy_from_user(buffData,buff,size);
	kstrtol(buffData,10,&signal_recu_max);
	

	return 0;
}


static int buffer_open(struct inode *in, struct file *f){
    printk(KERN_ALERT "OPEN\n");
    return 0;
}

static int buffer_release(struct inode *in, struct file *f){
    
    printk(KERN_ALERT "RELEASE\n");
    return 0;
}

static void buffer_cleanup(void){
	/* liberation */
	unregister_chrdev_region(dev,1);
	cdev_del(my_cdev);	
    //Relache l'acces aux GPIO
    gpio_free(PIN_SIGNAL);
	printk(KERN_ALERT "Goodbye \n");
}

module_init(buffer_init);
module_exit(buffer_cleanup);

MODULE_LICENSE(LICENCE);
MODULE_AUTHOR(AUTEUR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEVICE);
