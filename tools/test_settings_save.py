"""Exercise the actual settings writer with partial writes and storage failures."""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'epdiy-epub/src/reading_settings.cpp').read_text(encoding='utf-8')
writer = source[source.index('bool reading_settings_save(void)'):source.index('void reading_settings_load')]
stubs = r'''
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
#define SETTINGS_FILE_PATH "/settings.cfg"
#define O_WRONLY 1
#define O_CREAT 2
#define O_TRUNC 4
static int failure,closed,saved,write_calls;
static std::string content;
static int mock_open(const char *,int){return failure==1?-1:7;}
static int mock_write(int fd,const char *buf,int n){
    assert(fd==7);++write_calls;
    if(failure==2)return -1;
    if(failure==3)return 0;
    if(n>7)n=7;content.append(buf,n);return n;
}
static int mock_fsync(int fd){assert(fd==7);return failure==4?-1:0;}
static int mock_close(int fd){assert(fd==7);++closed;return failure==5?-1:0;}
static void rt_kprintf(const char *fmt,...){if(strstr(fmt,"[settings] Saved:"))++saved;}
static const char *font_manager_get_name(int){return "Default";}
static const int FONT_SIZES[]={32},BOLD_LEVELS[]={0},LINE_SPACINGS[]={12},MARGINS[]={8};
static int g_font_type_idx,g_font_size_idx,g_bold_idx,g_line_spacing_idx,g_margin_idx;
static unsigned g_auto_turn_seconds=15;
#define open mock_open
#define write mock_write
#define fsync mock_fsync
#define close mock_close
'''
main = r'''
int main(){
    for(failure=0;failure<=5;++failure){
        content.clear();closed=saved=write_calls=0;
        bool ok=reading_settings_save();
        assert(ok==(failure==0));assert(saved==(failure==0));
        assert(closed==(failure==1?0:1));
        if(!failure){assert(write_calls>1);assert(content==
          "font_name=Default\nfont_size=32\nbold=0\nline_spacing=12\nmargin=8\nauto_turn_seconds=15\n");}
    }
    puts("SETTINGS_PARTIAL_WRITE_AND_FAILURE_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='reader-settings-') as directory:
    path = Path(directory)
    cpp, exe = path / 'settings.cpp', path / 'settings.exe'
    cpp.write_text(stubs + writer + main, encoding='utf-8')
    subprocess.run([args.cxx, '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
