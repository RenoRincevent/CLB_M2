//scp TPEX.c root@vm-dyn-0-203:/root/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#define LICENCE "GPL"
#define AUTEUR "Emmanuel CAUSSE emmanuel.causse@univ-tlse3.fr"
#define DESCRIPTION "TP1 M2"
#define DEVICE "m1info5"
#define NB_SECTORS 1024
#define TAILLE_SECTEUR 512

#define SAMPLE_IOC_MAGIC 'k'
#define SAMPLE_IOCCRYPTED _IO(SAMPLE_IOC_MAGIC, 0)
#define SAMPLE_IOC_MAXNR 0

int init_rb(void);
static void cleanup_rb(void);
int rb_open(struct block_device *bdev, fmode_t fm);
void rb_close(struct gendisk *rb_disk, fmode_t fm);

int getSizeMdp(unsigned long pass);
int rb_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);

static void rb_request(struct request_queue *q);
int rb_transfer(struct request *req);

void * device_data;
const char DEV_NAME[] = "TP1_2";

static struct rb_device
{
    unsigned int size;
    spinlock_t lock;
    struct request_queue *rb_queue;
    struct gendisk *rb_disk;
}rb_dev;

static struct block_device_operations rb_fops =
{
    .owner = THIS_MODULE,
    .open = &rb_open,
    .release = &rb_close,
    //.getgeo = rb_getgeo,
    .ioctl = rb_ioctl,
};

int init_rb(void){
    int major;
    
    printk(KERN_ALERT "INIT\n");
    
    device_data = kmalloc(NB_SECTORS*TAILLE_SECTEUR,GFP_KERNEL);
    
    /* 1. Enregistrement du périphérique bloc au niveau du noyau */
    major = register_blkdev(0,DEV_NAME);
    if(major < 0){ 
		printk(KERN_ALERT "Erreur register_blkdev\n");
		return -EINVAL;
	}
	
	/* 2. Association d'une méthode request */
    spin_lock_init(&rb_dev.lock);
    rb_dev.rb_queue = blk_init_queue(&rb_request, &rb_dev.lock);
    
    if(rb_dev.rb_queue == NULL){
        printk(KERN_ALERT "Erreur blk_init_queue\n");
		return -EINVAL;
    }
	
	/* 3. Allocation + init de la structure gendisk */
    rb_dev.rb_disk = alloc_disk(9);
    
    if(rb_dev.rb_disk == NULL){
        printk(KERN_ALERT "Erreur alloc_disk\n");
		return -EINVAL;
    }
    rb_dev.rb_disk->major = major;
    rb_dev.rb_disk->first_minor = 0;
    rb_dev.rb_disk->minors = 9;
    snprintf(rb_dev.rb_disk->disk_name,32,DEV_NAME);
    rb_dev.rb_disk->fops = &rb_fops;
    rb_dev.rb_disk->queue = rb_dev.rb_queue;
    
    /* 4. Declaration de la capacite */
    rb_dev.size = NB_SECTORS;
    set_capacity(rb_dev.rb_disk, rb_dev.size);
    
    /* 5. Ajout du disque aux structure de donnees du noyau */
    add_disk(rb_dev.rb_disk);
    
	return 0;
}

static void cleanup_rb(void){
    unsigned int major;
    major = rb_dev.rb_disk->major;
    kfree(device_data);
    del_gendisk(rb_dev.rb_disk);
    put_disk(rb_dev.rb_disk);
    blk_cleanup_queue(rb_dev.rb_queue);
    unregister_blkdev(major,DEV_NAME);
    printk(KERN_ALERT "Goodbye \n");
}

int rb_open(struct block_device *bdev, fmode_t fm){
    //TODO
    return 0;
}

void rb_close(struct gendisk *rb_disk, fmode_t fm){
    //TODO
}

int getSizeMdp(unsigned long pass){
    int i,n;
    n = snprintf(NULL, 0, "%lu", pass);
    char mdp[n+1];
    snprintf(mdp, n+1, "%lu", pass);

    i=0;
    while(mdp[i] != '\0'){
        i++;
    }
    
    printk(KERN_ALERT "taille du mdp : %d\n",i);
    
    return i;
}

//arg = MDP
int rb_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg){
    if(_IOC_TYPE(cmd) != SAMPLE_IOC_MAGIC){return -ENOTTY;}
    if(_IOC_NR(cmd) > SAMPLE_IOC_MAXNR){return -ENOTTY;}
    
    unsigned long *data;
    int j,i,n;
    
    //Recup la taille du mdp et le convertit en string
    n = snprintf(NULL, 0, "%lu", arg);
    char mdp[n+1];
    snprintf(mdp, n+1, "%lu", arg);
    
    printk(KERN_ALERT "taille du mdp : %d\n",n);
    
    switch(cmd)
    {
        //Encrypt ou Decrypt les données grace a un XOR
        case SAMPLE_IOCCRYPTED:
            data = device_data;
            j = 0;
            for(i = 0; i<NB_SECTORS*TAILLE_SECTEUR; i++){
                data[i] = data[i] ^ mdp[j];
                j++;
                if(j >= n){j=0;}
            }
            memcpy(device_data,data,NB_SECTORS*TAILLE_SECTEUR);
            break;
        default:
            printk(KERN_ALERT "Unknown ioctl call\n");
            return -ENOTTY;
    }
    return 0;
}



static void rb_request(struct request_queue *q){
    struct request *req;
    int ret;
    
    /* Depiler les requetes */
    while((req = blk_fetch_request(q)) != NULL){
        /* Traiter la requete */
        ret = rb_transfer(req);
        __blk_end_request_all(req, ret);
    }
    
    
}

int rb_transfer(struct request *req){
    int sensOp;
    sector_t start_sector;
    int sectors_count;
    struct bio_vec bv;
    struct req_iterator iter;
    void *buffer;
    int sector_offset;
    int nSectors;
    
    /* 1. Determiner le sens de l'operation */
    sensOp = rq_data_dir(req);
    
    
    /* 2. Determiner le premier secteur a traiter */
    start_sector = blk_rq_pos(req);
    
    /* 3. Determiner le nombre de secteur a traiter */
    sectors_count = blk_rq_sectors(req);
    sector_offset = 0;
    /* 4. Traiter la requete en parcourant tout les segments de la requete */
    rq_for_each_segment(bv,req,iter){
        //Recupere l'adressse du buffer
        buffer = page_address(bv.bv_page) + bv.bv_offset;
        
        //Vérifie la longueur du buffer
        if(bv.bv_len % TAILLE_SECTEUR != 0){
            printk(KERN_ALERT "BAD SIZE buffer\n");
            return -EIO;
        }
        
        //Calcule le nombre de secteurs
        nSectors = (bv.bv_len) / TAILLE_SECTEUR;
        
        //Proceder au transfert
        if(sensOp == WRITE){ //=> write
            memcpy(buffer, device_data + (start_sector + sector_offset) * TAILLE_SECTEUR, TAILLE_SECTEUR);
        }
        else{ //=> read
            memcpy(device_data + (start_sector + sector_offset) * TAILLE_SECTEUR, buffer, TAILLE_SECTEUR);
        }
        sector_offset += nSectors;   
    }
    
    //Somme des sector_offset doit etre egale a sectors_count
    if(sector_offset != sectors_count){
        printk(KERN_ALERT "BAD SIZE sector_offset\n");
        return -EIO;
    }
    
    return 0;
}

module_init(init_rb);
module_exit(cleanup_rb);

MODULE_LICENSE(LICENCE);
MODULE_AUTHOR(AUTEUR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEVICE);
