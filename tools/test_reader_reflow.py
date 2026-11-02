"""Reflow real TextBlock/parser layout repeatedly; detect appended stale pages/lines."""
from pathlib import Path
import argparse, re, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'epdiy-epub/lib/Epub/RubbishHtmlParser'
text=(root/'blocks/TextBlock.cpp').read_text(encoding='utf-8')
text=text[text.index('void TextBlock::add_span'):text.index('void TextBlock::render')]
parser=(root/'RubbishHtmlParser.cpp').read_text(encoding='utf-8')
layout=parser[parser.index('void RubbishHtmlParser::layout_one_block'):parser.index('bool RubbishHtmlParser::render_text_preview')]
anchor=parser[parser.index('PageAnchor RubbishHtmlParser::get_page_anchor'):]
header=re.sub(r'^#(?:include|pragma).*$', '', (root/'blocks/TextBlock.h').read_text(encoding='utf-8'),flags=re.M)
stubs=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <list>
#include <string>
static int utf8_len(char c){return ((unsigned char)c<128)?1:3;}
#define ulog_i(...) ((void)0)
#define ulog_e(...) ((void)0)
#define rt_thread_delay(...) ((void)0)
#define rt_mutex_take(...) ((void)0)
#define rt_mutex_release(...) ((void)0)
#define RT_WAITING_FOREVER 0
class Epub {};
class Renderer {
public:
    int width=20, line_height=10;
    int get_page_width(){return width;}
    int get_line_height(){return line_height;}
    int get_page_height(){return 70;}
    int get_fixed_width_words(const char *s,const char **end,int n,bool,bool){
        int count=0;while(s[count]&&count<n)++count;*end=s+count;return count;
    }
};
enum BlockType { TEXT_BLOCK, IMAGE_BLOCK };
class Block {
public:
    virtual ~Block(){};
    virtual void layout(Renderer *,Epub *,int max_width=-1)=0;
    virtual BlockType getType()=0;
};
'''
support=r'''
void TextBlock::render(Renderer *,int,int,int){}
void TextBlock::dump(){}
class ImageBlock : public Block {
public:int height=20;
    void layout(Renderer *,Epub *,int=-1)override{}
    BlockType getType()override{return IMAGE_BLOCK;}
};
static int live_pages,live_elements;
class PageElement {public:PageElement(){++live_elements;}virtual ~PageElement(){--live_elements;}};
class PageLine:public PageElement {public:TextBlock *block;int line_break_index;
    PageLine(TextBlock *b,int l,int):block(b),line_break_index(l){}
};
class PageImage:public PageElement {public:ImageBlock *block;
    PageImage(ImageBlock *b,int):block(b){}
};
class Page {public:std::vector<PageElement *> elements;
    Page(){++live_pages;}~Page(){for(auto p:elements)delete p;--live_pages;}
};
struct PageAnchor {int block_index,line_index;};
class RubbishHtmlParser {
public:
    std::list<Block *> blocks;std::vector<Page *> pages;
    Renderer *m_layout_renderer;Epub *m_layout_epub;
    int m_layout_line_height,m_layout_page_height,m_layout_y,m_pages_mutex;
    bool m_layout_done;std::list<Block *>::iterator m_layout_iter;
    ~RubbishHtmlParser(){for(auto p:pages)delete p;for(auto b:blocks)delete b;}
    void layout_one_block(Block *);
    void layout(Renderer *,Epub *,int);
    void layout_continue(int);
    PageAnchor get_page_anchor(int);
    int find_page_by_anchor(const PageAnchor &,bool=false);
};
'''
tests=r'''
int main(){
    {
        Renderer r;Epub e;RubbishHtmlParser p;
        std::string original(500,'x');
        for(int i=0;i<4;++i){auto b=new TextBlock(LEFT_ALIGN);b->add_span(original.c_str(),false,false);p.blocks.push_back(b);}
        for(int round=0;round<12;++round){
            r.width=20+(round%3)*10;
            p.layout(&r,&e,1);
            assert(!p.m_layout_done);
            PageAnchor target={3,0};assert(p.find_page_by_anchor(target)==-1);
            while(!p.m_layout_done)p.layout_continue(1);
            assert(p.find_page_by_anchor(target)>0);
            int lines=(500+r.width-1)/r.width;
            for(auto block:p.blocks){
                auto b=static_cast<TextBlock *>(block);
                assert((int)b->line_breaks.size()==lines);
                std::string joined;
                for(auto l:b->line_breaks)joined.append(l.words_start,l.words_end);
                assert(joined==original);
            }
            assert(live_elements==4*lines);assert(live_pages==(int)p.pages.size());
            assert(p.get_page_anchor(0).block_index==0);
            for(int n=0;n<(int)p.pages.size();++n){auto anchor=p.get_page_anchor(n);assert(p.find_page_by_anchor(anchor,true)==n);}
        }
    }
    assert(live_pages==0&&live_elements==0);
    puts("READER_REPEAT_REFLOW_CONTENT_ANCHOR_LIFETIME_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='reader-reflow-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+header+text+support+layout+anchor+tests,encoding='utf-8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=20)
