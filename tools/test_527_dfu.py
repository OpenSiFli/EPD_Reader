"""Build and run the protected DFU core against simulated NOR and power cuts."""
from pathlib import Path
import argparse
import subprocess
import tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--gcc',default='gcc')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
common=root/'epdiy-epub/dfu527/common'
with tempfile.TemporaryDirectory(prefix='reader527-dfu-') as directory:
    exe=Path(directory)/'test.exe'
    subprocess.run([args.gcc,'-std=c99','-O2','-Wall','-Wextra','-Werror',
        '-I'+str(common),str(common/'reader_dfu.c'),str(root/'tools/test_527_dfu.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
