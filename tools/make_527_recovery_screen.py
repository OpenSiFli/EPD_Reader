"""Regenerate the recovery-only monochrome artwork (Pillow, explicit CJK font)."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

p=argparse.ArgumentParser()
p.add_argument('--font',type=Path,required=True)
p.add_argument('--preview',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
im=Image.new('1',(684,1216),1)
d=ImageDraw.Draw(im)
def text(y,value,size):
    font=ImageFont.truetype(str(a.font),size)
    bounds=d.textbbox((0,0),value,font=font)
    d.text(((684-(bounds[2]-bounds[0]))//2,y-bounds[1]),value,font=font,fill=0)
text(100,'S I F L I   R E A D E R',25)
d.line((292,170,392,170),fill=0,width=3)
text(290,'USB 升级模式',52)
text(392,'请连接电脑，运行升级工具',30)
d.rounded_rectangle((48,530,636,738),radius=14,outline=0,width=3)
text(575,'长按电源键 10 秒',40)
text(642,'重启并退出升级模式',32)
text(845,'升级过程中请保持 USB 连接',27)
text(905,'固件不完整时，重启仍会进入此模式',25)
text(950,'请重新连接电脑完成升级',27)
text(1080,'屏幕提示在进入升级模式时更新',22)
# Match EPD_ROT_INVERTED_PORTRAIT: native x=y, native y=683-x.
native=im.transpose(Image.Transpose.ROTATE_90)
out=root/'epdiy-epub/dfu527/loader/recovery_screen.bin'
out.write_bytes(native.tobytes())
assert out.stat().st_size==1216*684//8
if a.preview:
    a.preview.parent.mkdir(parents=True,exist_ok=True)
    im.save(a.preview)
print(out)
