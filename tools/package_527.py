"""Package a protected USB update, or an explicit first-install factory bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zlib
from usb_dfu_527 import BOARD,load_package


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=root / 'epdiy-epub/project/build_sf32lb527-oed-e0470a03_hcpu')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--factory',action='store_true',help='Include boot/DFU installation images; never use for routine USB updates')
    args = parser.parse_args()
    build = args.build.resolve()
    config = (build / 'rtconfig.h').read_text(encoding='utf-8')
    if '#define BSP_USING_BOARD_SF32LB527_OED_E0470A03' not in config:
        parser.error('Build directory is not the SF32LB527 E0470A03 board')
    specs = [
        (build / 'main.bin/ER_IROM1.bin', 0x12200000, 0x200000),
        (build / 'main.bin/ER_IROM2.bin', 0x12400000, 0xA00000),
        (root / 'epdiy-epub/waveform/e0470a03/epd_waveform.bin', 0x12EC0000, 0x40000),
    ]
    if args.factory:
        specs += [(build/'recovery/recovery.bin',0x12020000,0x180000),
                  (build/'ftab/ftab.bin',0x12000000,0x8000),
                  (build/'bootloader/bootloader.bin',0x12010000,0x10000)]
    images = []
    for source, address, limit in specs:
        data = source.read_bytes()
        if not 0 < len(data) <= limit:
            parser.error(f'{source.name}: image does not fit its partition')
        images.append(dict(path=source.name, address=hex(address), size=len(data),
                           sha256=hashlib.sha256(data).hexdigest(),crc32=zlib.crc32(data)))
    args.output.mkdir(parents=True, exist_ok=False)
    for source, _, _ in specs:
        shutil.copy2(source, args.output / source.name)
    update=dict(board=BOARD,layout=1,protocol=1,images=[dict(id=n,file=i['path'],size=i['size'],
        sha256=i['sha256'],crc32=i['crc32']) for n,i in enumerate(images[:3])])
    (args.output/'update.json').write_text(json.dumps(update,indent=2)+'\n',encoding='utf-8')
    load_package(args.output) # Validate vectors, hashes and board before publishing.
    if args.factory:
        words=[0x52444655,1,1,0x52454144,BOARD,1,3]
        for i in images[:3]:words += [i['size'],i['crc32']]
        record=struct.pack('<13I',*words);record+=struct.pack('<I',zlib.crc32(record))
        metadata=record+b'\xff'*(8192-len(record))
        (args.output/'update_state.bin').write_bytes(metadata)
        images.insert(4,dict(path='update_state.bin',address='0x12f00000',size=len(metadata),
            sha256=hashlib.sha256(metadata).hexdigest(),crc32=zlib.crc32(metadata)))
        manifest = dict(chip='SF32LB52', memory='NOR', write_flash=dict(verify=True,
                        files=[dict(path=i['path'], address=i['address']) for i in images]))
        (args.output / 'sftool_param.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        ini_lines = ['[FILEINFO]']
        for index, image in enumerate(images):
            ini_lines.extend((f"FILE{index}={image['path']}", f"ADDR{index}={image['address']}"))
        ini_lines.append(f'NUM={len(images)}')
        (args.output / 'ImgBurnList.ini').write_text('\n'.join(ini_lines) + '\n', encoding='utf-8')
    (args.output / 'images.json').write_text(json.dumps(images, indent=2) + '\n', encoding='utf-8')
    if args.factory:
        (args.output / 'README.txt').write_text(
            'SF32LB527 / E0470A03-AF-S, 684x1216 portrait\n'
            'FACTORY SERIAL INSTALL PACKAGE for blank Flash or deliberate full recovery.\n'
            'Read FACTORY_FIRST_FLASH.md before use; do not use this package for routine updates.\n'
            'Run flash_factory.bat COMx. sftool_param.json and ImgBurnList.ini are included.\n'
            'Recovery key: hold PA33 at boot. Hold the PA34 power key for 10 seconds to restart.\n'
            'The recovery loader includes its own on-screen USB upgrade instructions.\n'
            'images.json records every firmware image length, address, CRC32 and SHA-256.\n'
            'After installation, use the protected USB updater for routine firmware updates.\n', encoding='utf-8')
        (args.output / 'flash_factory.bat').write_text(
            '@echo off\n'
            'setlocal\n'
            'pushd "%~dp0"\n'
            'where sftool >nul 2>&1\n'
            'if errorlevel 1 (\n'
            '  echo ERROR: sftool was not found in PATH. Install the SiFli SDK tools first.\n'
            '  popd\n'
            '  exit /b 2\n'
            ')\n'
            'set "SERIAL_PORT=%~1"\n'
            'if not defined SERIAL_PORT set /p SERIAL_PORT=Enter SF32LB527 download UART port (for example COM36): \n'
            'if not defined SERIAL_PORT (\n'
            '  echo ERROR: no serial port was provided.\n'
            '  popd\n'
            '  exit /b 2\n'
            ')\n'
            'if /I not "%SERIAL_PORT:~0,3%"=="COM" set "SERIAL_PORT=COM%SERIAL_PORT%"\n'
            'echo Installing the SF32LB527 factory image set through %SERIAL_PORT%...\n'
            'sftool -p "%SERIAL_PORT%" -c SF32LB52 -m nor write_flash --verify ^\n'
            '  "ER_IROM1.bin@0x12200000" ^\n'
            '  "ER_IROM2.bin@0x12400000" ^\n'
            '  "epd_waveform.bin@0x12EC0000" ^\n'
            '  "recovery.bin@0x12020000" ^\n'
            '  "update_state.bin@0x12F00000" ^\n'
            '  "ftab.bin@0x12000000" ^\n'
            '  "bootloader.bin@0x12010000"\n'
            'set "FLASH_RC=%ERRORLEVEL%"\n'
            'if not "%FLASH_RC%"=="0" (\n'
            '  echo ERROR: serial flashing failed with exit code %FLASH_RC%.\n'
            '  popd\n'
            '  exit /b %FLASH_RC%\n'
            ')\n'
            'echo Flash write and verification completed. The tool will reset the board.\n'
            'popd\n'
            'exit /b 0\n', encoding='utf-8')
    else:
        (args.output / 'README.txt').write_text(
            'SF32LB527 / E0470A03-AF-S, 684x1216 portrait\n'
            'Routine USB update package for a board that already has the protected recovery loader.\n'
            'Enter USB upgrade from Settings, or hold the PA33 recovery key at boot.\n'
            'Run: python usb_dfu_527.py --package <this-directory> --port <native-USB-COM-port>\n'
            'update.json permits only application, font and waveform images.\n'
            'The recovery protocol cannot write its own loader, bootloader or Flash table.\n'
            'images.json records byte lengths and SHA-256. See DFU_SF32LB527.md for recovery/protection limits.\n', encoding='utf-8')
    shutil.copy2(root/'tools/usb_dfu_527.py',args.output/'usb_dfu_527.py')
    doc_names = ('DFU_SF32LB527.md','BOARD_SF32LB527.md') if args.factory else (
        'DFU_SF32LB527.md','BOARD_SF32LB527.md','VALIDATION_SF32LB527.md','VALIDATION_PREFETCH_527.md',
        'VALIDATION_WAVEPREP_527.md','VALIDATION_FONT_POWER_527.md','VALIDATION_POWER_DIAG_527.md',
        'MINIMUM_POWER_527.md','POWER_STEPS_527.md','TF_POWER_527.md','AUTO_TURN_527.md','STANDBY_CLOCK_527.md')
    if args.factory:
        doc_names += ('FACTORY_FIRST_FLASH.md',)
    for name in doc_names:
        shutil.copy2(root/name,args.output/name)
    print(f'Packaged {len(images)} images in {args.output.resolve()}')


if __name__ == '__main__':
    main()
