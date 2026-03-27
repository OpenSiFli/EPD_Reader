/**
 * @file ft_dfs_bridge.c
 * @brief FreeType 文件操作桥接（LRU 多槽缓冲版）
 *
 * 问题：FreeType 读取大字体(20MB)时在 loca/glyf/hmtx/cmap 四个表之间
 *       频繁跳跃读取。单缓冲窗口在表间跳转时立即失效。
 *
 * 方案：LRU 多槽缓冲 — 4 个独立的 16KB 缓冲槽，
 *       自然对应 loca/glyf/hmtx/cmap 四个表的访问区域。
 *       每个表的连续读取在各自的槽内命中，互不干扰。
 *
 *       预读低区保留（文件前 64KB），覆盖 FT_New_Face 阶段。
 *
 *       fast_seek：通过 SDK 标准接口 dfs_file_enable_fast_seek()
 *       启用 FatFs CLMT 簇链映射表，加速底层 lseek 的 FAT 链遍历。
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <dfs.h>
#include <dfs_file.h>
#include <string.h>
#include <stdlib.h>

/*==========================================================================
 * 缓冲区配置
 *========================================================================*/

#define FT_BUF_LO_SIZE     (64 * 1024)    /* 预读低区 */
#define FT_SLOT_SIZE        (4 * 1024)     /* 每个槽 4KB — 减少 miss 时的读取代价 */
#define FT_SLOT_COUNT       16              /* 16 个槽 — 更多槽位避免表间互相驱逐 */

typedef struct {
    long     start;                /* 缓冲的文件起始偏移，-1 = 空槽 */
    int      valid;                /* 有效字节数 */
    int      age;                  /* LRU 年龄，0 = 最新 */
    uint8_t  data[FT_SLOT_SIZE];
} FtBufSlot;

typedef struct {
    int        fd;
    long       file_size;
    long       file_pos;

    /* 低区：文件头部固定预读 */
    uint8_t    buf_lo[FT_BUF_LO_SIZE];
    int        buf_lo_valid;

    /* 高区：LRU 多槽 */
    FtBufSlot  slots[FT_SLOT_COUNT];
    int        age_counter;        /* 全局年龄计数器 */
} ft_file_t;

/*==========================================================================
 * 文件操作
 *========================================================================*/

struct dfs_fd* ft_fopen(const char* name, const char* mode)
{
    (void)mode;
    int fd = open(name, O_RDONLY);
    if (fd < 0) return NULL;

    /* 通过 SDK 标准接口启用 fast_seek（CLMT 簇链映射表）
     * fd_get 获取 dfs_fd 引用 → dfs_file_enable_fast_seek → fd_put 释放引用
     * 成功：后续 lseek 走 CLMT O(1) 查找，不再遍历 FAT 链
     * 失败：静默回退到普通 lseek，不影响功能 */
    {
        struct dfs_fd *dfs_fp = fd_get(fd);
        if (dfs_fp) {
            int ret = dfs_file_enable_fast_seek(dfs_fp, 1);
            if (ret >= 0) {
                rt_kprintf("[ft_io] fast_seek enabled (ret=%d)\n", ret);
            } else {
                rt_kprintf("[ft_io] fast_seek unavailable (ret=%d), using normal seek\n", ret);
            }
            fd_put(dfs_fp);
        }
    }

    ft_file_t *f = (ft_file_t *)rt_malloc(sizeof(ft_file_t));
    if (!f) { close(fd); return NULL; }

    f->fd = fd;
    f->file_pos = 0;
    f->age_counter = 0;

    /* 初始化所有槽为空 */
    for (int i = 0; i < FT_SLOT_COUNT; i++) {
        f->slots[i].start = -1;
        f->slots[i].valid = 0;
        f->slots[i].age = 0;
    }

    /* 获取文件大小 */
    f->file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    /* 预读文件头部 */
    int lo_to_read = (f->file_size < FT_BUF_LO_SIZE)
                   ? (int)f->file_size : FT_BUF_LO_SIZE;
    f->buf_lo_valid = read(fd, f->buf_lo, lo_to_read);
    if (f->buf_lo_valid < 0) f->buf_lo_valid = 0;

    rt_kprintf("[ft_io] Opened: size=%ld, preloaded=%d, slots=%dx%dKB\n",
               f->file_size, f->buf_lo_valid,
               FT_SLOT_COUNT, FT_SLOT_SIZE / 1024);

    return (struct dfs_fd *)f;
}

int ft_fclose(struct dfs_fd* fp)
{
    if (!fp) return -1;
    ft_file_t *f = (ft_file_t *)fp;
    close(f->fd);
    rt_free(f);
    return 0;
}

int ft_fseek(struct dfs_fd* fp, long offset, int whence)
{
    if (!fp) return -1;
    ft_file_t *f = (ft_file_t *)fp;

    long new_pos;
    switch (whence) {
    case SEEK_SET: new_pos = offset; break;
    case SEEK_CUR: new_pos = f->file_pos + offset; break;
    case SEEK_END: new_pos = f->file_size + offset; break;
    default: return -1;
    }

    f->file_pos = new_pos;
    return 0;
}

long ft_ftell(struct dfs_fd* fp)
{
    if (!fp) return -1;
    return ((ft_file_t *)fp)->file_pos;
}

/*--------------------------------------------------------------------------
 * 在多槽中查找命中的槽，返回槽索引，-1 = 未命中
 *------------------------------------------------------------------------*/
static int slot_find_hit(ft_file_t *f, long pos)
{
    for (int i = 0; i < FT_SLOT_COUNT; i++) {
        FtBufSlot *s = &f->slots[i];
        if (s->start >= 0 &&
            pos >= s->start &&
            pos <  s->start + s->valid)
        {
            /* 命中，更新 LRU 年龄 */
            s->age = ++f->age_counter;
            return i;
        }
    }
    return -1;
}

/*--------------------------------------------------------------------------
 * 找到最老的槽（用于淘汰），返回槽索引
 *------------------------------------------------------------------------*/
static int slot_find_oldest(ft_file_t *f)
{
    int oldest_idx = 0;
    int oldest_age = f->slots[0].age;
    for (int i = 1; i < FT_SLOT_COUNT; i++) {
        if (f->slots[i].start < 0) return i;  /* 空槽优先使用 */
        if (f->slots[i].age < oldest_age) {
            oldest_age = f->slots[i].age;
            oldest_idx = i;
        }
    }
    return oldest_idx;
}

/*--------------------------------------------------------------------------
 * 从文件填充指定槽
 *------------------------------------------------------------------------*/
static int slot_fill(ft_file_t *f, int slot_idx, long file_offset)
{
    FtBufSlot *s = &f->slots[slot_idx];

    if (lseek(f->fd, file_offset, SEEK_SET) < 0) {
        s->start = -1;
        s->valid = 0;
        return -1;
    }

    int n = read(f->fd, s->data, FT_SLOT_SIZE);
    if (n <= 0) {
        s->start = -1;
        s->valid = 0;
        return -1;
    }

    s->start = file_offset;
    s->valid = n;
    s->age = ++f->age_counter;
    return 0;
}

size_t ft_fread(void* ptr, size_t size, size_t nitems, struct dfs_fd* fp)
{
    if (!fp || !ptr || size == 0 || nitems == 0) return 0;
    ft_file_t *f = (ft_file_t *)fp;

    size_t total = size * nitems;
    size_t done = 0;
    uint8_t *dst = (uint8_t *)ptr;

    while (done < total) {
        size_t remain = total - done;
        long   pos    = f->file_pos;

        /* ---- 优先级 1: 低区缓冲命中 ---- */
        if (pos >= 0 && pos < f->buf_lo_valid) {
            int avail = f->buf_lo_valid - (int)pos;
            int to_copy = (int)remain < avail ? (int)remain : avail;
            memcpy(dst + done, f->buf_lo + pos, to_copy);
            done += to_copy;
            f->file_pos += to_copy;
            continue;
        }

        /* ---- 优先级 2: 多槽 LRU 命中 ---- */
        int hit = slot_find_hit(f, pos);
        if (hit >= 0) {
            FtBufSlot *s = &f->slots[hit];
            int buf_off = (int)(pos - s->start);
            int avail   = s->valid - buf_off;
            int to_copy = (int)remain < avail ? (int)remain : avail;
            memcpy(dst + done, s->data + buf_off, to_copy);
            done += to_copy;
            f->file_pos += to_copy;
            continue;
        }

        /* ---- 未命中：淘汰最老的槽，从文件填充 ---- */
        int victim = slot_find_oldest(f);
        if (slot_fill(f, victim, pos) < 0) break;
        /* 下次循环会从新填充的槽读取 */
    }

    return done / size;
}