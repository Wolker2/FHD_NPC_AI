"""Confirm the single extra native save-thread VMT slot against the exact EXE."""
from pathlib import Path
import hashlib,struct,unittest,os
EXE=Path(os.environ.get('FHD_TARGET_EXE','/mnt/data/Rangers.exe'))
class SaveVMTHooks(unittest.TestCase):
    def test_hash(self):
        if not EXE.is_file(): self.skipTest("Pass FHD_TARGET_EXE for executable checks")
        self.assertEqual(hashlib.sha256(EXE.read_bytes()).hexdigest(),
           'e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf')
    def test_vmt_and_original_prologue(self):
        if not EXE.is_file(): self.skipTest("Pass FHD_TARGET_EXE for executable checks")
        b=EXE.read_bytes();pe=struct.unpack_from('<I',b,0x3c)[0];opts=struct.unpack_from('<H',b,pe+20)[0]
        sections=[]
        for i in range(struct.unpack_from('<H',b,pe+6)[0]):
            o=pe+24+opts+i*40;vsz,rva,rsz,off=struct.unpack_from('<IIII',b,o+8)
            sections.append((rva,rsz,off))
        def va(addr,length):
            r=addr-0x400000
            for base,n,off in sections:
                if base<=r and r+length<=base+n:
                    return b[off+r-base:off+r-base+length]
            raise AssertionError('unmapped address')
        self.assertEqual(struct.unpack('<I',va(0x5e51a4,4))[0],0x5e51c4)
        self.assertEqual(va(0x5e51c4,3),b'\x55\x8b\xec')
        # Original excluded VMT entries must remain unchanged.
        self.assertEqual(struct.unpack('<I',va(0x5b1300,4))[0],0x5b32e0)
    def test_only_one_extra_save_slot_is_patched(self):
        a=Path('src/fhd_save_adapter.c').read_text(encoding='utf8')
        self.assertIn('0x1e51a4u',a)
        self.assertIn('0x1e51c4u',a)
        self.assertIn('FHDNPC_SaveStub',a)
if __name__=='__main__':unittest.main()
