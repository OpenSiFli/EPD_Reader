"""Update the SF32LB527 reader over native USB CDC. No raw-address or erase command.

The recovery image must first be installed with the factory bundle.
Install pyserial, enter USB upgrade from Settings (or hold PA33 for at least
two seconds after power-on/reset), then:
    python tools/usb_dfu_527.py --package firmware-527 --port COM42

Windows GUI (serial installation and native USB update): tools/flash_gui/README.md
"""
import argparse
import hashlib
import json
from pathlib import Path
import secrets
import struct
import time
import zlib

BOARD=0x05270470
MAGIC=0x55373235
NAMES=('ER_IROM1.bin','ER_IROM2.bin','epd_waveform.bin')
LIMITS=(0x200000,0xa00000,0x40000)

def load_package(path):
    meta=json.loads((path/'update.json').read_text(encoding='utf-8'))
    if meta.get('board')!=BOARD or meta.get('layout')!=1 or meta.get('protocol')!=1:
        raise ValueError('Wrong board, layout or protocol')
    records=meta['images']
    if len(records)!=3:raise ValueError('An update requires exactly three images')
    images=[]
    for i,(name,limit) in enumerate(zip(NAMES,LIMITS)):
        r=records[i]
        if r.get('id')!=i or r.get('file')!=name:raise ValueError('Unexpected update target')
        data=(path/name).read_bytes()
        if not 0<len(data)<=limit or len(data)!=r['size']:raise ValueError('Invalid image length')
        if hashlib.sha256(data).hexdigest()!=r['sha256'] or zlib.crc32(data)!=r['crc32']:
            raise ValueError('Image checksum mismatch: '+name)
        images.append(data)
    sp,pc=struct.unpack_from('<II',images[0])
    if sp%8 or not 0x20000000<=sp<=0x2007fc00 or not pc&1 or not 0x12200000<=pc<0x12200000+len(images[0]):
        raise ValueError('Invalid application vectors')
    manifest=struct.pack('<III',BOARD,1,3)+b''.join(struct.pack('<II',len(data),zlib.crc32(data)) for data in images)
    return images,manifest

def encode(command,sequence,payload=b''):
    header=struct.pack('<IIII',MAGIC,command,sequence,len(payload))
    crc=zlib.crc32(payload,zlib.crc32(header))
    return header+struct.pack('<I',crc)+payload

class Recovery:
    def __init__(self,port):
        import serial
        self.serial=serial.Serial(port=None,baudrate=1000000,timeout=.2,write_timeout=10)
        self.serial.rts=False;self.serial.dtr=False;self.serial.port=port;self.serial.open()
        self.serial.reset_input_buffer();self.serial.dtr=True
        time.sleep(.2)
        self.sequence=secrets.randbits(30)
    def exact(self,n,deadline):
        result=bytearray()
        while len(result)<n and time.monotonic()<deadline:
            result.extend(self.serial.read(n-len(result)))
        if len(result)!=n:raise TimeoutError('Recovery reply timed out; re-enter DFU and restart the complete update')
        return bytes(result)
    def command(self,command,payload=b'',timeout=30):
        self.sequence=(self.sequence+1)&0xffffffff
        frame=encode(command,self.sequence,payload)
        if self.serial.write(frame)!=len(frame):raise IOError('Short USB write')
        self.serial.flush();deadline=time.monotonic()+timeout
        header=self.exact(20,deadline)
        magic,op,seq,size,crc=struct.unpack('<IIIII',header)
        if (magic,op,seq,size)!=(MAGIC,command|0x80000000,self.sequence,12):raise ValueError('Unexpected recovery reply')
        body=self.exact(size,deadline)
        if zlib.crc32(body,zlib.crc32(header[:16]))!=crc:raise ValueError('Corrupt recovery reply')
        status,value,board=struct.unpack('<iII',body)
        if board!=BOARD:raise ValueError('Wrong device board ID')
        if status:raise RuntimeError(f'Recovery rejected command {command}: {status}; device remains in DFU')
        return value
    def close(self):self.serial.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path)
    parser.add_argument('--port',help='Native USB CDC port, auto-detected when omitted')
    parser.add_argument('--info',action='store_true')
    parser.add_argument('--reboot',action='store_true',help='Boot only if all installed image checksums pass')
    args=parser.parse_args()
    if not args.info and not args.reboot and args.package is None:parser.error('--package, --info or --reboot is required')
    images,manifest=load_package(args.package) if args.package else (None,None)
    from serial.tools import list_ports
    port=args.port
    if not port:
        candidates=[p.device for p in list_ports.comports() if p.vid==0x38f4 and p.pid==1 and p.serial_number=='EPD527-RECOVERY-1']
        if len(candidates)!=1:parser.error('Connect exactly one recovery device, or specify --port')
        port=candidates[0]
    device=Recovery(port)
    try:
        print(f'Recovery on {port}, state=0x{device.command(1):08x}',flush=True)
        if images:
            device.command(2,manifest)
            for i,data in enumerate(images):
                progress=-1
                for offset in range(0,len(data),2048):
                    block=data[offset:offset+2048]
                    received=device.command(3,struct.pack('<II',i,offset)+block)
                    if received!=offset+len(block):raise ValueError('Unexpected device write offset')
                    percent=received*100//len(data)
                    if percent//10!=progress:
                        progress=percent//10;print(f'{NAMES[i]}: {percent}%',flush=True)
            device.command(4,timeout=120)
            print('All images read back and verified; boot/DFU partitions were not update targets.',flush=True)
        if images or args.reboot:
            device.command(6,timeout=120);print('Reboot requested.',flush=True)
    finally:device.close()

if __name__=='__main__':main()
