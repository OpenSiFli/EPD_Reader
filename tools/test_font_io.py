"""Run the actual FreeType DFS cache with short reads and injected I/O failures."""
from pathlib import Path
import argparse, re, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--gcc',default='gcc');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
code=(root/'epdiy-epub/src/ft_dfs_bridge.c').read_text(encoding='utf-8')
code=re.sub(r'^#include.*$', '', code, flags=re.M)
stubs=r'''
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#define O_RDONLY 0
typedef unsigned rt_tick_t;
struct dfs_fd { int unused; };
static struct dfs_fd descriptor;
static unsigned char disk[300123];
static long cursor;
static int failure, read_limit, allocated, opened, reads;
static unsigned rt_tick_get(void){return 0;}
#define rt_kprintf(...) ((void)0)
static void *epub_mem_malloc(size_t n){if(failure==1)return NULL;void *p=malloc(n);if(p)++allocated;return p;}
static void epub_mem_free(void *p){if(p)--allocated;free(p);}
static int open(const char *p,int flags){if(failure==2)return -1;assert(!opened);opened=1;cursor=0;return 7;}
static int close(int fd){assert(fd==7&&opened);opened=0;return 0;}
static long lseek(int fd,long offset,int whence){
    assert(fd==7&&opened);if(failure==3)return -1;
    cursor=(whence==SEEK_END?sizeof(disk):whence==SEEK_CUR?cursor:0)+offset;
    assert(cursor>=0&&cursor<=(long)sizeof(disk));return cursor;
}
static int read(int fd,void *p,int n){
    assert(fd==7&&opened);++reads;
    if(failure==4)return -1;
    if(failure==5)return 0;
    if(read_limit&&n>read_limit)n=read_limit;
    if(n>(long)sizeof(disk)-cursor)n=sizeof(disk)-cursor;
    memcpy(p,disk+cursor,n);cursor+=n;return n;
}
static struct dfs_fd *fd_get(int fd){assert(fd==7);return &descriptor;}
static void fd_put(struct dfs_fd *fd){assert(fd==&descriptor);}
static int dfs_file_enable_fast_seek(struct dfs_fd *fd,int enable){return 0;}
'''
tests=r'''
static void check(struct dfs_fd *f,long offset,size_t n){
    unsigned char buffer[9000];assert(n<=sizeof(buffer));
    assert(ft_fseek(f,offset,SEEK_SET)==0);
    size_t expected=n;if(expected>sizeof(disk)-offset)expected=sizeof(disk)-offset;
    assert(ft_fread(buffer,1,n,f)==expected);
    assert(!memcmp(buffer,disk+offset,expected));assert(ft_ftell(f)==offset+(long)expected);
}
int main(void){
    for(unsigned i=0;i<sizeof(disk);++i)disk[i]=(unsigned char)(i^(i>>9));
    for(int short_reads=0;short_reads<2;++short_reads){
        read_limit=short_reads?37:0;failure=0;
        struct dfs_fd *f=ft_fopen("font.ttf","rb");assert(f&&allocated==1);
        check(f,0,7000);check(f,65530,8193);check(f,131000,8193);
        for(int i=0;i<500;++i)check(f,65536+(i*7919)%220000,53+i%500);
        check(f,290000,50);int before=reads;check(f,290020,20);assert(reads==before);
        check(f,sizeof(disk)-20,100);assert(ft_fread(disk,1,1,f)==0);
        long old=ft_ftell(f);assert(ft_fseek(f,LONG_MAX,SEEK_CUR)==-1);
        assert(ft_fseek(f,LONG_MIN,SEEK_CUR)==-1);assert(ft_fseek(f,-1,SEEK_SET)==-1);
        assert(ft_ftell(f)==old);assert(ft_fread(disk,(size_t)-1,2,f)==0);
        for(failure=3;failure<=5;++failure){
            assert(ft_fseek(f,80000+failure*4096,SEEK_SET)==0);
            /* Force a miss, including partial windows that previously looped forever. */
            ft_file_t *cache=(ft_file_t *)f;
            for(int i=0;i<FT_SLOT_COUNT;++i)cache->slots[i].start=-1;
            assert(ft_fread(disk,1,1,f)==0);
        }
        failure=0;assert(ft_fclose(f)==0);assert(!allocated&&!opened);
    }
    for(failure=1;failure<=3;++failure){assert(!ft_fopen("font.ttf","rb"));assert(!allocated&&!opened);}
    puts("FONT_IO_ALIGNMENT_SHORT_READ_EOF_FAILURE_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='font-io-') as d:
    d=Path(d);src=d/'test.c';exe=d/'test.exe'
    src.write_text(stubs+code+tests,encoding='utf-8')
    subprocess.run([a.gcc,'-O2','-Wall',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=20)
