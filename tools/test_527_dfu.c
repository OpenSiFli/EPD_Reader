/* Host regression of the production DFU storage core; no device is accessed. */
#include "reader_dfu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BASE 0x12000000u
#define FLASH_SIZE 0x1000000u
static uint8_t flash[FLASH_SIZE],snapshot[FLASH_SIZE],images[3][10000];
static int operations,fail_operation,fail_after;
static int allowed(uint32_t address,uint32_t size)
{
    if(address==R527_META0||address==R527_META1)return size<=R527_SECTOR;
    for(unsigned i=0;i<3;++i) {
        uint32_t start=r527_image_base(i),limit=r527_image_limit(i);
        if(address>=start&&address-start<=limit&&size<=limit-(address-start))return 1;
    }
    return 0;
}
static int read_nor(uint32_t a,void *p,uint32_t n)
{assert(a>=BASE&&a-BASE<=FLASH_SIZE&&n<=FLASH_SIZE-(a-BASE));memcpy(p,flash+a-BASE,n);return 0;}
static int erase_nor(uint32_t a,uint32_t n)
{
    assert(allowed(a,n)&&a%4096==0&&n%4096==0);++operations;
    if(operations==fail_operation){memset(flash+a-BASE,255,fail_after?n/2:0);return -1;}
    memset(flash+a-BASE,255,n);return 0;
}
static int write_nor(uint32_t a,const void *p,uint32_t n)
{
    assert(allowed(a,n));++operations;const uint8_t *v=p;
    unsigned end=operations==fail_operation?(fail_after?n/2:0):n;
    for(unsigned i=0;i<end;++i){assert((flash[a-BASE+i]&v[i])==v[i]);flash[a-BASE+i]=v[i];}
    return operations==fail_operation?-1:0;
}
static const r527_io io={read_nor,write_nor,erase_nor};
static r527_manifest manifest(unsigned variant)
{
    r527_manifest m={.board=R527_BOARD_ID,.layout=R527_LAYOUT_ID,.count=3};
    unsigned sizes[]={8193,6001,1025};
    for(unsigned i=0;i<3;++i) {
        for(unsigned n=0;n<sizes[i];++n)images[i][n]=(n*17+i*71+variant*33)&255;
        if(i==0){uint32_t v[]={0x20060000u,0x12200101u};memcpy(images[0],v,8);}
        m.image[i].size=sizes[i];m.image[i].crc=r527_crc(images[i],sizes[i]);
    }
    return m;
}
static int update(const r527_manifest *m)
{
    r527_session s;int rc=r527_begin(&io,&s,m);if(rc)return rc;
    for(unsigned i=0;i<3;++i)for(unsigned offset=0;offset<m->image[i].size;) {
        unsigned n=m->image[i].size-offset;if(n>997)n=997; /* crosses erase/page boundaries */
        rc=r527_data(&io,&s,i,offset,images[i]+offset,n);if(rc)return rc;offset+=n;
    }
    return r527_commit(&io,&s);
}
static int bootable(void)
{r527_record r;return r527_latest(&io,&r,0)==0&&r527_verify(&io,&r)==0;}
int main(void)
{
    assert(sizeof(r527_manifest)==36&&sizeof(r527_record)==56);
    assert(r527_crc("123456789",9)==0xcbf43926u);
    memset(flash,0xa5,sizeof(flash));memset(flash+R527_META0-BASE,255,8192);
    r527_manifest m=manifest(0);assert(update(&m)==0&&bootable());
    memcpy(snapshot,flash,sizeof(flash));m=manifest(1);operations=0;
    assert(update(&m)==0&&bootable());int count=operations;
    for(int partial=0;partial<2;++partial)for(int failure=1;failure<=count;++failure) {
        memcpy(flash,snapshot,sizeof(flash));operations=0;fail_operation=failure;fail_after=partial;
        assert(update(&m)!=0);
        /* A valid boot decision must still describe the complete old image. */
        if(bootable())for(unsigned i=0;i<3;++i)
            assert(!memcmp(flash+r527_image_base(i)-BASE,snapshot+r527_image_base(i)-BASE,m.image[i].size));
        assert(!memcmp(flash,snapshot,0x200000)); /* boot/DFU/partition table untouched */
        fail_operation=0;operations=0;assert(update(&m)==0&&bootable());
    }
    printf("DFU_POWER_CUT_PASS points=%d modes=before/partial\n",count);
    r527_session s;operations=0;r527_manifest bad=m;bad.board^=1;
    assert(r527_begin(&io,&s,&bad)==R527_RANGE&&operations==0);
    bad=m;bad.image[0].size=0xffffffffu;
    assert(r527_begin(&io,&s,&bad)==R527_RANGE&&operations==0);
    assert(r527_begin(&io,&s,&m)==0);int before=operations;
    assert(r527_data(&io,&s,3,0,images[0],10)==R527_RANGE);
    assert(r527_data(&io,&s,0,0xffffffffu,images[0],10)==R527_RANGE);
    assert(r527_data(&io,&s,0,0,images[0],0xffffffffu)==R527_RANGE);
    assert(r527_data(&io,&s,0,1,images[0],10)==R527_RANGE);
    assert(r527_commit(&io,&s)==R527_INCOMPLETE&&operations==before&&!bootable());
    for(unsigned i=0;i<10000;++i)
        assert(r527_data(&io,&s,0x12000000u+i,0,images[0],10)==R527_RANGE);
    assert(operations==before);puts("DFU_RANGE_AND_PROTECTION_PASS");
    assert(update(&m)==0&&bootable());
    for(unsigned i=0;i<3;++i) {
        uint32_t a=r527_image_base(i)-BASE+10;flash[a]^=1;assert(!bootable());flash[a]^=1;
    }
    assert(bootable());puts("DFU_IMAGE_CORRUPTION_PASS");return 0;
}
