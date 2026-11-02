"""Capture a stable framebuffer through the reader's UI thread (COM36, 1Mbaud)."""
import argparse,re,time
from pathlib import Path
import serial
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--page',choices=['home','library','settings','light','reading'])
a=p.parse_args()
s=serial.Serial(port=None,baudrate=1000000,timeout=.1,write_timeout=3)
s.rts=False;s.dtr=False;s.port=a.port;s.open()
data=bytearray()
def send(command):
    for c in (command+'\r').encode('ascii'):s.write(bytes([c]));time.sleep(.005)
def until(marker,timeout):
    result=bytearray();deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        b=s.read(16384);result.extend(b);data.extend(b)
        if marker in result:return bytes(result)
        if b'HardFault' in result or b'Assertion failed' in result:raise RuntimeError('board fault')
    raise TimeoutError(marker)
try:
    if a.page:
        send('ui '+a.page);until(b'UI_DEBUG_DONE',90)
    send('ui dump');dump=until(b'FB_END fnv=',90)
    # Finish the hash line if the transport split it at the marker.
    deadline=time.monotonic()+2
    while not re.search(rb'FB_END fnv=[0-9a-f]{8}',dump) and time.monotonic()<deadline:
        b=s.read(4096);dump+=b;data.extend(b)
finally:s.close()
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.with_suffix('.log').write_bytes(data)
frame=bytearray(1216*684//2);seen=set()
for at,h in re.findall(rb'FB:([0-9a-f]{6}) ([0-9a-f]+)',dump):
    offset=int(at,16);chunk=bytes.fromhex(h.decode())
    assert len(chunk)==32 and offset+len(chunk)<=len(frame)
    frame[offset:offset+len(chunk)]=chunk;seen.add(offset)
assert len(seen)==len(frame)//32, (len(seen),len(frame)//32)
hash_value=2166136261
for value in frame:hash_value=((hash_value^value)*16777619)&0xffffffff
expected=int(re.search(rb'FB_END fnv=([0-9a-f]{8})',dump)[1],16)
assert hash_value==expected,(hex(hash_value),hex(expected))
pixels=bytes(v for b in frame for v in ((b&15)*17,(b>>4)*17))
Image.frombytes('L',(1216,684),pixels).rotate(-90,expand=True).save(a.output)
print(f'SCREEN_CAPTURE_PASS {a.output} 684x1216 fnv={hash_value:08x}')
