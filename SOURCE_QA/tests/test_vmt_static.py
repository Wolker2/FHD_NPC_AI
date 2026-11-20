import os
from pathlib import Path
import hashlib,struct,subprocess,sys

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('FHD_TARGET_EXE', '/mnt/data/Rangers.exe'))

def test_reference_exe_sha():
    assert hashlib.sha256(EXE.read_bytes()).hexdigest()=='e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'

def test_seven_known_vmt_signatures():
    out=subprocess.run([sys.executable,str(ROOT/'tests/verify_target.py'),str(EXE)],capture_output=True,text=True)
    assert out.returncode==0,out.stderr
    assert out.stdout.count('"verified": true')==6

def test_hash_mismatch_fails_closed(tmp_path):
    corrupted=bytearray(EXE.read_bytes());corrupted[-1]^=1
    p=tmp_path/'Rangers_bad.exe';p.write_bytes(corrupted)
    out=subprocess.run([sys.executable,str(ROOT/'tests/verify_target.py'),str(p)],capture_output=True,text=True)
    assert out.returncode!=0

def test_forbidden_kling_hook_absent():
    s=(ROOT/'src/fhd_vmt_probe.c').read_text()
    # No VMT address of TKling/Dominator. There are ONLY six hooks, not seven.
    assert '0x1b1300' not in s
    assert '0x1b32e0' not in s
    assert '#define COUNT 6u' in s
    assert 'type==0u||type==12u||owner==5u' in s

def test_no_direct_edit_of_exe():
    code='\n'.join(p.read_text() for p in (ROOT/'src').glob('*.c'))
    assert 'WriteProcessMemory(' not in code
    assert 'VirtualProtect' in code
    assert 'FHDNPC_DisarmObserver' in code

def test_raw_executable_untouched():
    assert EXE.stat().st_size>4_000_000
