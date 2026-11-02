"""Check packaged image bounds and host protocol encoding without USB hardware."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import zlib
from usb_dfu_527 import BOARD,NAMES,encode,load_package

def main():
    with tempfile.TemporaryDirectory(prefix='reader527-update-') as directory:
        root=Path(directory)
        arrays=[struct.pack('<II',0x20002000,0x12200009)+bytes(range(128)),b'font-data'*20,b'wave-data'*20]
        meta=dict(board=BOARD,layout=1,protocol=1,images=[])
        for i,(name,data) in enumerate(zip(NAMES,arrays)):
            (root/name).write_bytes(data)
            meta['images'].append(dict(id=i,file=name,size=len(data),sha256=hashlib.sha256(data).hexdigest(),crc32=zlib.crc32(data)))
        def save(): (root/'update.json').write_text(json.dumps(meta))
        def rejected():
            try:load_package(root)
            except (ValueError,KeyError):return
            raise AssertionError('Invalid package accepted')
        save();images,manifest=load_package(root);assert images==arrays
        frame=encode(2,9,manifest);assert len(frame)==56
        assert struct.unpack_from('<I',frame,16)[0]==zlib.crc32(frame[20:],zlib.crc32(frame[:16]))
        meta['board']^=1;save();rejected();meta['board']=BOARD
        meta['images'][0]['file']='bootloader.bin';save();rejected();meta['images'][0]['file']=NAMES[0]
        meta['images'][0]['id']=0x12000000;save();rejected();meta['images'][0]['id']=0
        meta['images'][0]['size']=0xffffffff;save();rejected();meta['images'][0]['size']=len(arrays[0]);save()
        (root/NAMES[1]).write_bytes(b'wrong data');rejected()
    print('DFU_PACKAGE_TARGETS_HASHES_AND_WIRE_PASS')

if __name__=='__main__':main()
