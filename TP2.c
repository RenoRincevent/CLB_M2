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

#define PIN_TRIGGER 17
#define PIN_ECHO 27
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
	if(alloc_chrdev_region(&dev,0,1,"module_rw") == -1){
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
    gpio_request_one(PIN_TRIGGER,GPIOF_OUT_INIT_LOW,"trigger");
    gpio_request_one(PIN_ECHO,GPIOF_IN,"echo");

	return 0;
}

static ssize_t buffer_read(struct file *f, char *buff, size_t size, loff_t *offp){
    unsigned long distance;
    struct timespec start,end,duree;
    char distanceStr[50];
    int nbChar;
    printk(KERN_ALERT "Read called!\n");
    nbChar = 0;
    
    //Déclenche le signal par un signal de démarrage de 10 us
    gpio_set_value(PIN_TRIGGER,HIGH);
    udelay(10);
    gpio_set_value(PIN_TRIGGER,LOW);
    
    //Lorsque le pin echo est a 0, le message a été envoyé
    while( gpio_get_value(PIN_ECHO) == 0 );
    getnstimeofday(&start);
    
    //tant que le que pin echo est a 1 alors il n'a toujours pas recu le signal
    while( gpio_get_value(PIN_ECHO) == 1 );
    getnstimeofday(&end);

    //Signal recu
    duree = timespec_sub(end,start);
    printk("Intervalle de temps : %lu msec\n",duree.tv_nsec/1000000);
    
    distance = (duree.tv_nsec/1000)*34300/2;
    
    //Le capteur marche entre 2cm et 300cm 
    if ((distance/1000000)<2 || (distance/1000000)>300){
        nbChar = sprintf(distanceStr, "Distance hors de portee\n");
    }
    else{
        nbChar = sprintf(distanceStr, "Distance de l'obstacle: %lu cm\n", distance/1000000);
    }
    
    //Envoie de la distance
    if(copy_to_user(buff, distanceStr, nbChar) != 0){
        return -EFAULT;
    }
    
    //Pause avant la prochaine mesure 
    mdelay(800);

    return nbChar;
}

static ssize_t buffer_write(struct file *f, const char * buff, size_t size, loff_t *offp){
	printk(KERN_ALERT "Write called\n");

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
    gpio_free(PIN_TRIGGER);
    gpio_free(PIN_ECHO);
	printk(KERN_ALERT "Goodbye \n");
}

module_init(buffer_init);
module_exit(buffer_cleanup);

MODULE_LICENSE(LICENCE);
MODULE_AUTHOR(AUTEUR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEVICE);
