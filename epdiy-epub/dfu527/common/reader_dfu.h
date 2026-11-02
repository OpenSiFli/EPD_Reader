/* SPDX-License-Identifier: Apache-2.0 */
#ifndef READER527_DFU_H
#define READER527_DFU_H
#include <stdint.h>
#include <stddef.h>

#define R527_BOARD_ID 0x05270470u
#define R527_LAYOUT_ID 1u
#define R527_LOADER_BASE 0x12020000u
#define R527_LOADER_SIZE 0x00180000u
#define R527_META0 0x12f00000u
#define R527_META1 0x12f01000u
#define R527_SECTOR 4096u
#define R527_COOKIE 0x527df001u
#define R527_MAGIC 0x52444655u
#define R527_WIRE_MAGIC 0x55373235u
#define R527_MAX_PAYLOAD 4096u
#define R527_IMAGE_COUNT 3u
#define R527_READY 0x52454144u
#define R527_UPDATING 0x55504454u

enum { R527_INFO=1,R527_BEGIN,R527_DATA,R527_COMMIT,R527_ABORT,R527_REBOOT };
enum { R527_OK=0,R527_BAD_REQUEST=-1,R527_RANGE=-2,R527_IO=-3,
       R527_CRC=-4,R527_STATE=-5,R527_INCOMPLETE=-6 };

/* All wire and NOR structures use little-endian uint32_t words. */
typedef struct { uint32_t size,crc; } r527_image;
typedef struct {
    uint32_t board,layout,count;
    r527_image image[R527_IMAGE_COUNT];
} r527_manifest;
typedef struct {
    uint32_t magic,version,sequence,state;
    r527_manifest manifest;
    uint32_t crc;
} r527_record;
typedef struct {
    int (*read)(uint32_t,void *,uint32_t);
    int (*write)(uint32_t,const void *,uint32_t);
    int (*erase)(uint32_t,uint32_t);
} r527_io;
typedef struct {
    r527_manifest manifest;
    uint32_t received[R527_IMAGE_COUNT];
    int active;
} r527_session;

uint32_t r527_crc_update(uint32_t crc,const void *data,size_t size);
uint32_t r527_crc(const void *data,size_t size);
uint32_t r527_image_base(unsigned image);
uint32_t r527_image_limit(unsigned image);
int r527_manifest_valid(const r527_manifest *manifest);
int r527_record_valid(const r527_record *record);
int r527_latest(const r527_io *io,r527_record *record,unsigned *slot);
int r527_verify(const r527_io *io,const r527_record *record);
int r527_begin(const r527_io *io,r527_session *session,const r527_manifest *manifest);
int r527_data(const r527_io *io,r527_session *session,unsigned image,uint32_t offset,const void *data,uint32_t size);
int r527_commit(const r527_io *io,r527_session *session);
#endif
