#include "standby_clock.h"
#include "EpubList/State.h"
#include "EpubList/EpubReader.h"
#include <stddef.h>
#include "type.h"
#include <dfs_posix.h>
#include <string.h>
extern EpubListState epub_list_state;
extern AppUIState ui_state;
extern int g_last_read_index;
extern EpubReader *reader;
extern bool reader527_standby_resume_requested();
extern "C" int board_tf_present(void);

struct ResumeRecord {uint32_t magic,bytes,reading,last;int anchor_block,anchor_line;EpubListState state;uint32_t checksum;};
static int saved_block,saved_line;
static uint32_t checksum(const ResumeRecord &r)
{
    const uint8_t *p=(const uint8_t *)&r;uint32_t hash=2166136261u;
    for(unsigned n=0;n<offsetof(ResumeRecord,checksum);++n)hash=(hash^p[n])*16777619u;
    return hash;
}
int reader527_standby_save()
{
    if(reader)reader->pause_background();
    if(board_tf_present()!=1)return ui_state==READING_EPUB?-1:0;
    ResumeRecord record={};record.magic=0x53434c31;record.bytes=sizeof(record);
    record.reading=ui_state==READING_EPUB;record.last=(uint32_t)g_last_read_index;
    if(record.reading&&reader)reader->save_anchor(record.anchor_block,record.anchor_line);
    record.state=epub_list_state;record.checksum=checksum(record);
    int fd=open("/.standby-reader",O_WRONLY|O_CREAT|O_TRUNC,0);if(fd<0)return -1;
    unsigned offset=0;bool ok=true;
    while(offset<sizeof(record)) {
        int n=write(fd,((const uint8_t *)&record)+offset,sizeof(record)-offset);
        if(n<=0){ok=false;break;}offset+=(unsigned)n;
    }
    if(ok&&fsync(fd))ok=false;
    if(close(fd))ok=false;
    rt_kprintf("CLOCK_BOOKMARK_SAVE rc=%d reading=%u\n",ok?0:-1,record.reading);return ok?0:-1;
}
bool reader527_standby_restore()
{
    if(!reader527_standby_resume_requested())return false;
    ResumeRecord record={};int fd=open("/.standby-reader",O_RDONLY);if(fd<0)return false;
    unsigned offset=0;
    while(offset<sizeof(record)) {
        int n=read(fd,((uint8_t *)&record)+offset,sizeof(record)-offset);if(n<=0)break;offset+=(unsigned)n;
    }
    close(fd);
    if(offset!=sizeof(record)||record.magic!=0x53434c31||record.bytes!=sizeof(record)||
       record.reading>1||record.anchor_block<0||record.anchor_line<0||record.checksum!=checksum(record)||record.state.num_epubs<0||
       record.state.num_epubs>MAX_EPUB_LIST_SIZE)return false;
    for(int n=0;n<record.state.num_epubs;++n)
        if(!memchr(record.state.epub_list[n].path,0,MAX_PATH_SIZE)||!memchr(record.state.epub_list[n].title,0,MAX_TITLE_SIZE))return false;
    if(record.state.num_epubs&& (record.state.selected_item<0||record.state.selected_item>=record.state.num_epubs))return false;
    epub_list_state=record.state;
    saved_block=record.anchor_block;saved_line=record.anchor_line;
    g_last_read_index=record.last<(unsigned)record.state.num_epubs?(int)record.last:-1;
    rt_kprintf("CLOCK_BOOKMARK_RESTORE reading=%u count=%d\n",record.reading,record.state.num_epubs);
    return record.reading&&record.state.num_epubs>0;
}
bool reader527_standby_render_saved(Renderer *r)
{
    EpubListItem &item=epub_list_state.epub_list[epub_list_state.selected_item];
    reader=new EpubReader(item,r);
    if(!reader->load()){delete reader;reader=nullptr;return false;}
    reader->restore_by_anchor(saved_block,saved_line,true);reader->render();return true;
}
