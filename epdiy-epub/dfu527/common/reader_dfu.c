/* SPDX-License-Identifier: Apache-2.0
 * Flash addresses are selected here, never supplied by the USB host.
 * Neither image writes nor journal writes can reach the boot/DFU partitions.
 */
#include "reader_dfu.h"
#include <string.h>

uint32_t r527_crc_update(uint32_t crc,const void *data,size_t size)
{
    static const uint32_t table[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,
        0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,0xedb88320,0xf00f9344,
        0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
    const uint8_t *p=data;
    while(size--){crc^=*p++;crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15];}
    return crc;
}
uint32_t r527_crc(const void *data,size_t size){return ~r527_crc_update(~0u,data,size);}
uint32_t r527_image_base(unsigned i)
{
    static const uint32_t bases[]={0x12200000u,0x12400000u,0x12ec0000u};
    return i<R527_IMAGE_COUNT?bases[i]:0;
}
uint32_t r527_image_limit(unsigned i)
{
    static const uint32_t limits[]={0x200000u,0xa00000u,0x40000u};
    return i<R527_IMAGE_COUNT?limits[i]:0;
}
int r527_manifest_valid(const r527_manifest *m)
{
    if(m->board!=R527_BOARD_ID||m->layout!=R527_LAYOUT_ID||m->count!=R527_IMAGE_COUNT)return 0;
    for(unsigned i=0;i<R527_IMAGE_COUNT;++i)
        if(!m->image[i].size||m->image[i].size>r527_image_limit(i))return 0;
    return m->image[0].size>=8;
}
int r527_record_valid(const r527_record *r)
{
    return r->magic==R527_MAGIC&&r->version==1&&
        (r->state==R527_READY||r->state==R527_UPDATING)&&
        r527_manifest_valid(&r->manifest)&&r->crc==r527_crc(r,offsetof(r527_record,crc));
}
int r527_latest(const r527_io *io,r527_record *out,unsigned *slot)
{
    r527_record a,b;
    int va=io->read(R527_META0,&a,sizeof(a))==0&&r527_record_valid(&a);
    int vb=io->read(R527_META1,&b,sizeof(b))==0&&r527_record_valid(&b);
    if(!va&&!vb)return R527_STATE;
    unsigned choose=vb&&(!va||(int32_t)(b.sequence-a.sequence)>0);
    *out=choose?b:a;if(slot)*slot=choose;return R527_OK;
}
static int journal(const r527_io *io,const r527_manifest *m,uint32_t state)
{
    r527_record old,r,verify;unsigned slot=1;
    uint32_t seq=r527_latest(io,&old,&slot)==0?old.sequence+1:1;
    memset(&r,0,sizeof(r));r.magic=R527_MAGIC;r.version=1;r.sequence=seq;
    r.state=state;r.manifest=*m;r.crc=r527_crc(&r,offsetof(r527_record,crc));
    uint32_t address=slot?R527_META0:R527_META1;
    /* Keep the previous valid record until the new one is fully verified. */
    if(io->erase(address,R527_SECTOR)||io->write(address,&r,sizeof(r))||
       io->read(address,&verify,sizeof(verify))||memcmp(&verify,&r,sizeof(r)))return R527_IO;
    return R527_OK;
}
int r527_verify(const r527_io *io,const r527_record *r)
{
    uint8_t buffer[512];uint32_t vector[2];
    if(!r527_record_valid(r)||r->state!=R527_READY)return R527_STATE;
    if(io->read(r527_image_base(0),vector,sizeof(vector)))return R527_IO;
    if((vector[0]&7)||vector[0]<0x20000000u||vector[0]>0x2007fc00u||
       !(vector[1]&1)||vector[1]<r527_image_base(0)||
       vector[1]>=r527_image_base(0)+r->manifest.image[0].size)return R527_RANGE;
    for(unsigned i=0;i<R527_IMAGE_COUNT;++i) {
        uint32_t crc=~0u,size=r->manifest.image[i].size;
        for(uint32_t off=0;off<size;) {
            uint32_t n=size-off;if(n>sizeof(buffer))n=sizeof(buffer);
            if(io->read(r527_image_base(i)+off,buffer,n))return R527_IO;
            crc=r527_crc_update(crc,buffer,n);off+=n;
        }
        if(~crc!=r->manifest.image[i].crc)return R527_CRC;
    }
    return R527_OK;
}
int r527_begin(const r527_io *io,r527_session *s,const r527_manifest *m)
{
    if(!r527_manifest_valid(m))return R527_RANGE;
    s->active=0;
    /* Commit UPDATING before any target erase. A power loss stays in recovery. */
    int rc=journal(io,m,R527_UPDATING);if(rc)return rc;
    memset(s,0,sizeof(*s));s->manifest=*m;s->active=1;return R527_OK;
}
int r527_data(const r527_io *io,r527_session *s,unsigned image,uint32_t offset,const void *data,uint32_t size)
{
    if(!s->active)return R527_STATE;
    if(image>=R527_IMAGE_COUNT||!size||size>R527_MAX_PAYLOAD-8)return R527_RANGE;
    uint32_t total=s->manifest.image[image].size;
    if(offset!=s->received[image]||offset>total||size>total-offset)return R527_RANGE;
    uint32_t base=r527_image_base(image);
    /* Strictly sequential writes: erase each sector once, including crossings. */
    uint32_t first=(offset+R527_SECTOR-1)&~(R527_SECTOR-1);
    for(uint32_t erase=first;erase<offset+size;erase+=R527_SECTOR)
        if(io->erase(base+erase,R527_SECTOR)){s->active=0;return R527_IO;}
    if(io->write(base+offset,data,size)){s->active=0;return R527_IO;}
    uint8_t verify[256];const uint8_t *source=data;
    for(uint32_t off=0;off<size;) {
        uint32_t n=size-off;if(n>sizeof(verify))n=sizeof(verify);
        if(io->read(base+offset+off,verify,n)||memcmp(verify,source+off,n)){s->active=0;return R527_IO;}
        off+=n;
    }
    s->received[image]+=size;return R527_OK;
}
int r527_commit(const r527_io *io,r527_session *s)
{
    if(!s->active)return R527_STATE;
    for(unsigned i=0;i<R527_IMAGE_COUNT;++i)
        if(s->received[i]!=s->manifest.image[i].size)return R527_INCOMPLETE;
    r527_record r;memset(&r,0,sizeof(r));r.magic=R527_MAGIC;r.version=1;
    r.state=R527_READY;r.manifest=s->manifest;r.crc=r527_crc(&r,offsetof(r527_record,crc));
    int rc=r527_verify(io,&r);if(rc){s->active=0;return rc;}
    rc=journal(io,&s->manifest,R527_READY);s->active=0;return rc;
}
