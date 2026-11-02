"""Exercise real standby bookmark IO and reject corrupt or truncated records."""
from pathlib import Path
import argparse,subprocess,tempfile,re
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
s=(root/'epdiy-epub/src/boards/sf32lb527/standby_resume.cpp').read_text(encoding='utf8')
state=(root/'epdiy-epub/lib/Epub/EpubList/State.h').read_text(encoding='utf8').replace('#pragma once','')
stubs=r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <vector>
#define rt_kprintf(...) ((void)0)
#define O_WRONLY 1
#define O_CREAT 2
#define O_TRUNC 4
#define O_RDONLY 8
class Renderer{};
enum {MAIN_PAGE,READING_EPUB};
static int ui_state=READING_EPUB,g_last_read_index=0;
static EpubListState epub_list_state={};
class EpubReader {public:
    EpubReader(){}EpubReader(EpubListItem &,Renderer *){}
    void pause_background(){}void save_anchor(int &b,int &l){b=42;l=9;}
    bool load(){return true;}void restore_by_anchor(int b,int l,bool exact){assert(b==42&&l==9&&exact);}void render(){}
};
static EpubReader *reader;
static bool resume_requested=true;static int present=1,failure,closed;static unsigned pos;
static std::vector<uint8_t> bytes;
static bool reader527_standby_resume_requested(){return resume_requested;}
static int board_tf_present(){return present;}
static int open(const char *,int mode,int=0){if(failure==1)return -1;pos=0;if(mode&O_TRUNC)bytes.clear();return 7;}
static int write(int fd,const void *p,unsigned size){assert(fd==7);if(failure==2)return -1;unsigned n=size>23?23:size;const uint8_t *b=(const uint8_t *)p;bytes.insert(bytes.end(),b,b+n);return n;}
static int read(int fd,void *p,unsigned size){assert(fd==7);if(failure==5)return -1;unsigned left=bytes.size()-pos,n=left<size?left:size;if(n>17)n=17;memcpy(p,bytes.data()+pos,n);pos+=n;return n;}
static int fsync(int){return failure==3?-1:0;}
static int close(int){++closed;return failure==4?-1:0;}
'''
actual=s[s.index('struct ResumeRecord'):]
main_source=(root/'epdiy-epub/src/main.cpp').read_text(encoding='utf8')
for match in re.finditer(r'delete contents;',main_source):
    assert re.match(r'\s*contents\s*=\s*nullptr;',main_source[match.end():])
reader_source=(root/'epdiy-epub/lib/Epub/EpubList/EpubReader.cpp').read_text(encoding='utf8')
load_function=reader_source[reader_source.index('bool EpubReader::load()'):reader_source.index('void EpubReader::parse_and_layout_current_section')]
tests=r'''
int main(){
    epub_list_state.num_epubs=1;epub_list_state.selected_item=0;epub_list_state.is_loaded=true;
    strcpy(epub_list_state.epub_list[0].path,"/book.epub");strcpy(epub_list_state.epub_list[0].title,"Book");
    epub_list_state.epub_list[0].current_section=7;epub_list_state.epub_list[0].current_page=23;
    EpubReader original;reader=&original;
    for(failure=1;failure<=4;++failure){closed=0;assert(reader527_standby_save()!=0);assert(closed==(failure==1?0:1));}
    failure=0;assert(reader527_standby_save()==0);auto good=bytes;
    epub_list_state={};assert(reader527_standby_restore());assert(epub_list_state.epub_list[0].current_page==23);
    reader=nullptr;Renderer r;assert(reader527_standby_render_saved(&r));delete reader;reader=nullptr;
    failure=5;assert(!reader527_standby_restore());failure=0;
    for(unsigned n=0;n<good.size();n+=31){bytes.assign(good.begin(),good.begin()+n);assert(!reader527_standby_restore());}
    bytes=good;bytes[20]^=1;assert(!reader527_standby_restore());
    for(unsigned bad=0;bad<4;++bad){
        ResumeRecord record;memcpy(&record,good.data(),sizeof(record));
        if(bad==0)record.state.num_epubs=MAX_EPUB_LIST_SIZE+1;
        if(bad==1)record.state.selected_item=1;
        if(bad==2)memset(record.state.epub_list[0].path,'x',MAX_PATH_SIZE);
        if(bad==3)record.anchor_block=-1;
        record.checksum=checksum(record);bytes.assign((uint8_t *)&record,(uint8_t *)&record+sizeof(record));
        assert(!reader527_standby_restore());
    }
    bytes=good;resume_requested=false;assert(!reader527_standby_restore());resume_requested=true;
    present=0;assert(reader527_standby_save()!=0);ui_state=MAIN_PAGE;assert(!reader527_standby_save());
    puts("STANDBY_BOOKMARK_SHORT_IO_FAILURE_CORRUPTION_ANCHOR_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='standby-resume-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe';cpp.write_text(state+stubs+actual+tests,encoding='utf8')
    subprocess.run([a.cxx,'-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
    cpp=d/'load.cpp';exe=d/'load.exe'
    cpp.write_text(r'''
#include <cassert>
#include <string>
#define ulog_d(...) ((void)0)
static bool load_result;
struct Epub {std::string path;Epub(const std::string &p):path(p){}const std::string &get_path(){return path;}bool load(){return load_result;}};
struct Renderer {void show_busy(){}};
class EpubReader {public:Epub *epub=nullptr;int *parser=nullptr;Renderer *renderer;struct {std::string path="/book.epub";}state;void stop_layout_thread(){}bool load();};
'''+load_function+r'''
int main(){Renderer r;EpubReader a;a.renderer=&r;load_result=true;assert(a.load());delete a.epub;
EpubReader b;b.renderer=&r;load_result=false;assert(!b.load());delete b.epub;}
''',encoding='utf8')
    subprocess.run([a.cxx,'-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
    print('EPUB_LOAD_SUCCESS_FAILURE_RETURN_CONTRACT_PASS')
