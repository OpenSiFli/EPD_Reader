/* SPDX-License-Identifier: Apache-2.0
 * COM34/COM36 original: SPI1 PA24/25/28/29, AW9523B GE0 detect.
 * No automatic formatting. Reader settings and filesystem writes are enabled.
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <finsh.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <dfs_posix.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define DIR FATFS_DIR
#include "ff.h"
#undef DIR
#include "drv_spi.h"
#include "spi_msd.h"
#include "epd_tps.h"
#include "tf_card.h"
#include "board527.h"

static struct rt_mutex lock;
static unsigned started,mounted,ready,failures;
static rt_device_t card;
static struct rt_device_blk_geometry geometry;
ALIGN(32) static uint8_t block[4096],compare[4096];
static uint32_t hash_bytes(uint32_t h,const void *data,unsigned n)
{
    const uint8_t *p=data;
    while(n--)h=(h^*p++)*16777619u;
    return h;
}
static int list_root(int verbose,uint32_t *fingerprint,unsigned *entries)
{
    DIR *d=opendir("/");
    if(!d)return -1;
    struct dirent *item;
    uint32_t sum=0;unsigned count=0;
    int rc=0;
    while((item=readdir(d))) {
        if(!strcmp(item->d_name,".")||!strcmp(item->d_name,".."))continue;
        char path[300];struct stat st;
        if(rt_snprintf(path,sizeof(path),"/%s",item->d_name)>=(int)sizeof(path)||stat(path,&st)){rc=-1;break;}
        uint32_t h=hash_bytes(2166136261u,item->d_name,strlen(item->d_name));
        h=hash_bytes(h,&st.st_size,sizeof(st.st_size));
        sum+=h;++count;
        if(verbose && count<=64)rt_kprintf("TF_ENTRY %s %u %s\n",S_ISDIR(st.st_mode)?"dir":"file",(unsigned)st.st_size,item->d_name);
    }
    if(closedir(d))rc=-1;
    if(fingerprint)*fingerprint=sum;
    if(entries)*entries=count;
    rt_kprintf("TF_DIRECTORY entries=%u name_size_fingerprint=%08x rc=%d\n",count,sum,rc);
    return rc;
}
static int mount_card(void)
{
    if(mounted)return 0;
    if(dfs_mount("sd0","/","elm",0,RT_NULL)) {
        rt_kprintf("TF_MOUNT_FAIL errno=%d; existing media retained\n",errno);return -1;
    }
    mounted=1;
    rt_kprintf("TF_MOUNT_PASS root=/ filesystem=elm (FAT/exFAT) writes_enabled=1\n");
    return 0;
}
static int unmount_card(void)
{
    if(!mounted)return 0;
    if(dfs_unmount("/"))return -1;
    mounted=0;return 0;
}
static int initialize(void)
{
    int detect=board_tf_present();
    rt_thread_mdelay(20);
    if(detect!=1 || board_tf_present()!=1) {
        rt_kprintf("TF_NO_CARD present=%d detect=AW_GE0\n",detect);return -1;
    }
    if(mounted)return 0;
    struct rt_spi_device *spi=(struct rt_spi_device *)rt_device_find("tfspi");
    if(!spi) {
        if(rt_hw_spi_device_attach("spi1","tfspi")!=RT_EOK)return -1;
        spi=(struct rt_spi_device *)rt_device_find("tfspi");
        if(!spi || rt_device_open((rt_device_t)spi,RT_DEVICE_OFLAG_RDWR)!=RT_EOK)return -1;
    }
    tf_msd_write_enable(1);
    card=rt_device_find("sd0");
    if(board527_io_enter())return -RT_EIO;
    int rc=card?msd_reinit():msd_init("sd0","tfspi");
    board527_io_exit();
    if(rc){ready=0;rt_kprintf("TF_INIT_FAIL rc=%d\n",rc);return -1;}
    card=rt_device_find("sd0");
    if(!card || rt_device_control(card,RT_DEVICE_CTRL_BLK_GETGEOME,&geometry)!=RT_EOK || geometry.bytes_per_sector!=512 || geometry.sector_count<4096)return -1;
    ready=1;
    rt_kprintf("TF_READY sectors=%u sector_bytes=%u capacity_MiB=%u SPI1 PA24/25/28/29\n",
        geometry.sector_count,geometry.bytes_per_sector,geometry.sector_count/2048);
    tf_msd_status();
    return mount_card();
}
static int status(void)
{
    rt_kprintf("TF_STATUS ready=%u mounted=%u present=%d failures=%u sectors=%u sector_bytes=%u\n",
        ready,mounted,board_tf_present(),failures,geometry.sector_count,geometry.bytes_per_sector);
    tf_msd_status();
    if(mounted) {
        struct dfs_filesystem *mounted_fs=dfs_filesystem_lookup("/");
        if(mounted_fs && mounted_fs->data) {
            FATFS *fat=(FATFS *)mounted_fs->data;
            rt_kprintf("TF_FILESYSTEM type=%u (1=FAT12 2=FAT16 3=FAT32 4=exFAT) cluster_sectors=%u volume_lba=%u\n",
                fat->fs_type,fat->csize,(unsigned)fat->volbase);
        }
        struct statfs fs;
        if(statfs("/",&fs))return -1;
        rt_kprintf("TF_SPACE block_bytes=%u blocks=%u free_blocks=%u\n",(unsigned)fs.f_bsize,(unsigned)fs.f_blocks,(unsigned)fs.f_bfree);
    }
    return 0;
}
static int read_test(void)
{
    if(!ready||board_tf_present()!=1)return -1;
    const uint32_t lbas[]={0,2048,geometry.sector_count/2,geometry.sector_count-8};
    for(unsigned n=0;n<4;++n) {
        if(rt_device_read(card,lbas[n],block,8)!=8 || rt_device_read(card,lbas[n],compare,8)!=8 || memcmp(block,compare,sizeof(block)))return -1;
        rt_kprintf("TF_SAMPLE lba=%u sectors=8 hash=%08x double_read=PASS\n",lbas[n],hash_bytes(2166136261u,block,sizeof(block)));
    }
    uint32_t hashes[2],times[2];
    for(unsigned pass=0;pass<2;++pass) {
        uint32_t begin=rt_tick_get(),h=2166136261u;
        for(unsigned lba=0;lba<2048;lba+=8) {
            if(rt_device_read(card,lba,block,8)!=8)return -1;
            h=hash_bytes(h,block,sizeof(block));
            rt_thread_mdelay(1); /* Yield between bounded blocks. */
        }
        hashes[pass]=h;times[pass]=rt_tick_get()-begin;
    }
    if(hashes[0]!=hashes[1])return -1;
    rt_kprintf("TF_READ_PASS bytes_each=1048576 passes=2 hash=%08x ms=%u/%u (includes CRC/hash/yield)\n",hashes[0],times[0],times[1]);
    return 0;
}
static uint8_t pattern(uint32_t offset,uint32_t seed)
{
    uint32_t v=(offset>>2)^seed;v^=v<<13;v^=v>>17;v^=v<<5;
    return v>>((offset&3)*8);
}
static int verify_file(const char *path,unsigned bytes,uint32_t seed)
{
    int fd=open(path,O_RDONLY);if(fd<0)return -1;
    unsigned offset=0;int rc=0;
    while(offset<bytes) {
        unsigned n=bytes-offset;if(n>sizeof(block))n=sizeof(block);
        if(read(fd,block,n)!=(int)n){rc=-1;break;}
        for(unsigned i=0;i<n;++i)if(block[i]!=pattern(offset+i,seed)){rc=-1;break;}
        if(rc)break;
        offset+=n;rt_thread_mdelay(1);
    }
    if(!rc && read(fd,block,1)!=0)rc=-1;
    if(close(fd))rc=-1;
    rt_kprintf("TF_FILE_VERIFY bytes=%u rc=%d\n",offset,rc);
    return rc;
}
static int write_test(void)
{
    if(!mounted||board_tf_present()!=1)return -1;
    uint32_t before=0,after=0;unsigned nb=0,na=0;
    struct statfs free_before,free_after;
    if(list_root(0,&before,&nb)||statfs("/",&free_before))return -1;
    if((uint64_t)free_before.f_bfree*free_before.f_bsize<2u*1024*1024)return -1;
    const unsigned bytes=1024*1024;
    uint32_t seed=rt_tick_get()^0x52754466;
    char path[32];int fd=-1,created=0,rc=-1;
    tf_msd_write_enable(1);
    for(unsigned tries=0;tries<16;++tries) {
        rt_snprintf(path,sizeof(path),"/TF%06X.BIN",(seed+tries)&0xffffff);
        fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0);
        if(fd>=0){created=1;break;}
        if(errno!=EEXIST)goto cleanup;
    }
    if(!created)goto cleanup;
    rt_kprintf("TF_TEST_FILE path=%s bytes=%u seed=%08x\n",path,bytes,seed);
    uint32_t begin=rt_tick_get();
    for(unsigned offset=0;offset<bytes;offset+=sizeof(block)) {
        for(unsigned i=0;i<sizeof(block);++i)block[i]=pattern(offset+i,seed);
        if(write(fd,block,sizeof(block))!=sizeof(block))goto cleanup;
        rt_thread_mdelay(1);
    }
    if(fsync(fd))goto cleanup;
    if(close(fd)){fd=-1;goto cleanup;}fd=-1;
    rt_kprintf("TF_FILE_WRITE bytes=%u ms=%u (includes pattern/fsync/yield)\n",bytes,(unsigned)(rt_tick_get()-begin));
    tf_msd_write_enable(1);
    if(unmount_card()||mount_card()||verify_file(path,bytes,seed))goto cleanup;
    /* Reinitialize the card/host and mount again before a second full read. */
    if(unmount_card()||initialize()||verify_file(path,bytes,seed))goto cleanup;
    rc=0;
cleanup:
    if(fd>=0 && close(fd))rc=-1;
    if(created) {
        tf_msd_write_enable(1);
        if(!mounted || unlink(path)) {
            rt_kprintf("TF_TEST_FILE_RETAINED %s\n",path);rc=-1;
        }
    }
    tf_msd_write_enable(1);
    if(!rc && (unmount_card()||mount_card()||list_root(0,&after,&na)||statfs("/",&free_after)))rc=-1;
    if(!rc && (before!=after||nb!=na||free_before.f_bfree!=free_after.f_bfree))rc=-1;
    rt_kprintf("TF_WRITE_TEST %s before=%08x after=%08x entries=%u/%u (original file contents not traversed)\n",rc?"FAIL":"PASS",before,after,nb,na);
    return rc;
}
static int file_hash(const char *path)
{
    if(!mounted)return -1;
    int fd=open(path,O_RDONLY);if(fd<0)return -1;
    uint32_t hash=2166136261u,total=0,begin=rt_tick_get();int n=0,rc=0;
    while(total<8u*1024*1024 && (n=read(fd,block,sizeof(block)))>0) {
        hash=hash_bytes(hash,block,n);total+=n;rt_thread_mdelay(1);
    }
    if(n<0)rc=-1;
    if(close(fd))rc=-1;
    rt_kprintf("TF_FILE_READ rc=%d bytes=%u hash=%08x ms=%u capped_at_8MiB=%u\n",rc,total,hash,(unsigned)(rt_tick_get()-begin),total==8u*1024*1024);
    return rc;
}
/* Keep the same writable descriptor across real VDD cycles; verify offsets,
 * readback without unmounting the reader's live font/book descriptors. */
static int power_test(void)
{
    if(!mounted||board_tf_present()!=1)return -1;
    uint32_t before=0,after=0;unsigned nb=0,na=0;
    struct statfs fb,fa;
    if(list_root(0,&before,&nb)||statfs("/",&fb)||(uint64_t)fb.f_bfree*fb.f_bsize<1048576)return -1;
    uint32_t seed=rt_tick_get()^0x52737000u;char path[32];int fd=-1,created=0,rc=-1;
    board527_storage_pm_enable(1);
    for(unsigned n=0;n<16;++n) {
        rt_snprintf(path,sizeof(path),"/TP%06X.BIN",(seed+n)&0xffffff);
        fd=open(path,O_RDWR|O_CREAT|O_EXCL,0);
        if(fd>=0){created=1;break;}if(errno!=EEXIST)return -1;
    }
    if(!created)return -1;
    for(unsigned cycle=0;cycle<16;++cycle) {
        unsigned bytes=(cycle+1)*sizeof(block);
        for(unsigned i=0;i<sizeof(block);++i)block[i]=pattern(cycle*sizeof(block)+i,seed);
        if(write(fd,block,sizeof(block))!=sizeof(block)||fsync(fd))goto pm_cleanup;
        int sleep_rc=-RT_EBUSY;
        for(unsigned tries=0;tries<60&&sleep_rc==-RT_EBUSY;++tries) {
            sleep_rc=board527_storage_idle(1);
            if(sleep_rc==-RT_EBUSY)rt_thread_mdelay(100);
        }
        if(sleep_rc)goto pm_cleanup;
        rt_thread_mdelay(40);
        if(lseek(fd,0,SEEK_SET)!=0)goto pm_cleanup;
        for(unsigned offset=0;offset<bytes;offset+=sizeof(compare)) {
            if(read(fd,compare,sizeof(compare))!=sizeof(compare))goto pm_cleanup;
            for(unsigned i=0;i<sizeof(compare);++i)if(compare[i]!=pattern(offset+i,seed))goto pm_cleanup;
        }
        rt_kprintf("TF_PM_FILE_CYCLE cycle=%u bytes=%u PASS\n",cycle+1,bytes);
    }
    if(close(fd)){fd=-1;goto pm_cleanup;}fd=-1;
    if(verify_file(path,16*sizeof(block),seed))goto pm_cleanup;
    rc=0;
pm_cleanup:
    if(fd>=0&&close(fd))rc=-1;
    if(!rc&&unlink(path))rc=-1;
    if(rc)rt_kprintf("TF_PM_TEST_FILE_RETAINED %s\n",path);
    if(!rc&&(list_root(0,&after,&na)||statfs("/",&fa)||before!=after||nb!=na||fb.f_bfree!=fa.f_bfree))rc=-1;
    rt_kprintf("TF_PM_FILE_TEST rc=%d cycles=16 retained_fd=1 directory_preserved=%d\n",rc,!rc);
    return rc;
}
static int tf(int argc,char **argv)
{
    if(!started)return -RT_ENOSYS;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);int rc=-RT_EINVAL;
    if(argc==2) {
        if(!strcmp(argv[1],"status"))rc=status();
        else if(!strcmp(argv[1],"init"))rc=initialize();
        else if(!strcmp(argv[1],"ls"))rc=mounted?list_root(1,RT_NULL,RT_NULL):-1;
        else if(!strcmp(argv[1],"readtest"))rc=read_test();
        else if(!strcmp(argv[1],"writetest"))rc=write_test();
        else if(!strcmp(argv[1],"pmtest"))rc=power_test();
        else if(!strcmp(argv[1],"unmount"))rc=unmount_card();
    } else if(argc==3&&!strcmp(argv[1],"read"))rc=file_hash(argv[2]);
    if(rc) {
        ++failures;
        rt_kprintf("TF_COMMAND_FAIL rc=%d errno=%d\n",rc,errno);
        if(rc==-RT_EINVAL)rt_kprintf("tf status | init | ls | readtest | writetest | pmtest | unmount | read <path>\n");
    }
    rt_mutex_release(&lock);return rc;
}
MSH_CMD_EXPORT(tf, TF card diagnostic (no format));
static void startup(void *arg)
{
    (void)arg;rt_mutex_take(&lock,RT_WAITING_FOREVER);
    int rc=initialize();if(rc)++failures;
    status();rt_kprintf("TF_BOOT rc=%d; use tf init after inserting a card\n",rc);
    rt_mutex_release(&lock);
    if(!rc)board527_storage_pm_enable(1);
}
int tf_card_start(void)
{
    rt_mutex_init(&lock,"tflock",RT_IPC_FLAG_PRIO);
    rt_thread_t thread=rt_thread_create("tfinit",startup,RT_NULL,8192,24,10);
    if(!thread)return -RT_ENOMEM;
    started=1;rt_thread_startup(thread);return 0;
}
void tf_card_display_lock(void)
{
    if(started) {
        rt_mutex_take(&lock,RT_WAITING_FOREVER);
        struct rt_spi_device *spi=(struct rt_spi_device *)rt_device_find("tfspi");
        if(spi)rt_mutex_take(&spi->bus->lock,RT_WAITING_FOREVER);
    }
}
void tf_card_display_unlock(void)
{
    if(started) {
        struct rt_spi_device *spi=(struct rt_spi_device *)rt_device_find("tfspi");
        if(spi)rt_mutex_release(&spi->bus->lock);
        rt_mutex_release(&lock);
    }
}

int board527_storage_mount(void)
{
    if(!started){rt_mutex_init(&lock,"tflock",RT_IPC_FLAG_PRIO);started=1;}
    rt_mutex_take(&lock,RT_WAITING_FOREVER);int rc=initialize();rt_mutex_release(&lock);
    if(!rc)board527_storage_pm_enable(1);return rc;
}

int board527_storage_test_begin(void)
{
    board527_storage_pm_enable(0);
    if(!started)return -RT_ENOSYS;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);
    dfs_lock();
    struct dfs_filesystem *fs=dfs_filesystem_lookup("/");
    struct dfs_fdtable *table=dfs_fdtable_get();
    int busy=0;
    for(unsigned i=0;i<table->maxfd;++i) {
        struct dfs_fd *fd=table->fds[i];
        if(fs&&fd&&fd->ref_count>0&&fd->fs==fs){busy=1;break;}
    }
    if(busy||unmount_card()) {
        dfs_unlock();rt_mutex_release(&lock);return -RT_EBUSY;
    }
    if(board527_io_diagnostic_begin()){dfs_unlock();rt_mutex_release(&lock);return -RT_EIO;}
    struct rt_spi_device *spi=(struct rt_spi_device *)rt_device_find("tfspi");
    if(spi)rt_mutex_take(&spi->bus->lock,RT_WAITING_FOREVER);
    ready=0;
    /* UI owns the locks until remount; diagnostics/background opens cannot
     * touch the unpowered card. No user data is written or formatted. */
    return 0;
}
int board527_storage_test_end(void)
{
    int rc=initialize();
    struct rt_spi_device *spi=(struct rt_spi_device *)rt_device_find("tfspi");
    if(spi)rt_mutex_release(&spi->bus->lock);
    board527_io_diagnostic_end();dfs_unlock();rt_mutex_release(&lock);
    if(!rc)board527_storage_pm_enable(1);return rc;
}
