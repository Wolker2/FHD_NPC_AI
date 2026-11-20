"""FHD 0.9 tagged SAV memory sidecar Linux integration tests; not a Windows game test."""
import ctypes as C
import pathlib
import tempfile
import unittest
from test_bridge import DLL,ship,put,record,encoded_path

KEY=0x4648444e12345678
DLL.FHD_BridgeSaveTaggedSlot.argtypes=[C.POINTER(C.c_uint16),C.c_uint64,C.c_void_p,C.c_uint32,C.c_uint64]
DLL.FHD_BridgeSaveTaggedSlot.restype=C.c_int32
DLL.FHD_BridgeLoadTaggedSlot.argtypes=[C.POINTER(C.c_uint16),C.c_uint64,C.c_uint64]
DLL.FHD_BridgeLoadTaggedSlot.restype=C.c_int32
DLL.FHD_Count.restype=C.c_uint32
DLL.FHD_BridgeMoveTaggedSlotIfMatch.argtypes=[C.POINTER(C.c_uint16),C.POINTER(C.c_uint16),C.c_uint64]
DLL.FHD_BridgeMoveTaggedSlotIfMatch.restype=C.c_int32

def tag(path):
    h1=2166136261; h2=5381
    for b in path.read_bytes():
        h1=((h1 ^ b)*16777619)&0xffffffff
        h2=(((h2<<5)+h2)^b)&0xffffffff
    return (h1<<32)|h2

def save(path, key=KEY, savtag=None):
    need=DLL.FHD_Snapshot(None,0)
    buf=(C.c_uint8*need)()
    assert DLL.FHD_Snapshot(buf,need)==need
    return DLL.FHD_BridgeSaveTaggedSlot(encoded_path(path),key,buf,need,tag(path) if savtag is None else savtag)

def load(path,key=KEY,savtag=None):
    return DLL.FHD_BridgeLoadTaggedSlot(encoded_path(path),key,tag(path) if savtag is None else savtag)

class Tagged09(unittest.TestCase):
    def setUp(self):DLL.FHD_BridgeNewGame()
    def test_memory_paired_to_exact_file(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'one.sav';p.write_bytes(b'RSG:one')
            self.assertEqual(put(ship()),1)
            self.assertEqual(record(101,99,1),1)
            before=DLL.FHD_Relation(101,99)
            self.assertEqual(save(p),1)
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(p),1)
            self.assertEqual(DLL.FHD_Relation(101,99),before)
    def test_another_sav_cannot_load_same_sidecar(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'one.sav';p.write_bytes(b'RSG:first')
            put(ship());record(101,99,1);self.assertEqual(save(p),1)
            p.write_bytes(b'RSG:different')
            self.assertLess(load(p),0)
            self.assertEqual(DLL.FHD_Count(),0)
    def test_wrong_galaxy_key_clears_memory(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'a.sav';p.write_bytes(b'RSG:save')
            put(ship());self.assertEqual(save(p),1)
            self.assertLess(load(p,key=KEY+1),0)
            self.assertEqual(DLL.FHD_Count(),0)
    def test_damage_to_sidecar_clears_memory(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'a.sav';p.write_bytes(b'RSG:save')
            put(ship());self.assertEqual(save(p),1)
            side=pathlib.Path(str(p)+'.fhdnpc');b=bytearray(side.read_bytes());b[-1]^=0xff;side.write_bytes(b)
            self.assertLess(load(p),0);self.assertEqual(DLL.FHD_Count(),0)
    def test_missing_sidecar_never_uses_previous_session(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'missing.sav';p.write_bytes(b'RSG:save')
            put(ship());self.assertLess(load(p),0);self.assertEqual(DLL.FHD_Count(),0)
    def test_failed_write_preserves_old_sidecar(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'a.sav';p.write_bytes(b'RSG:save')
            put(ship());self.assertEqual(save(p),1)
            old=pathlib.Path(str(p)+'.fhdnpc').read_bytes()
            missing=pathlib.Path(t)/'no_dir'/'a.sav';missing_parent=pathlib.Path(str(missing)+'.fhdnpc')
            self.assertLess(save(missing,savtag=123),0)
            self.assertFalse(missing_parent.exists())
            self.assertEqual(pathlib.Path(str(p)+'.fhdnpc').read_bytes(),old)
    def test_quicksave_rotation_without_sidecar_does_not_load_wrong_memory(self):
        with tempfile.TemporaryDirectory() as t:
            first=pathlib.Path(t)/'QuickSave.sav';second=pathlib.Path(t)/'QuickSave2.sav'
            first.write_bytes(b'RSG:quick1');second.write_bytes(b'RSG:quick2')
            put(ship());self.assertEqual(save(first),1)
            # Engine moves the SAV, but this version doesn't rotate sidecars;
            # load must NOT silently attach incorrect memories.
            first.replace(second)
            self.assertLess(load(second),0)
            self.assertEqual(DLL.FHD_Count(),0)
    def test_verified_quicksave_slot_migration(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'QuickSave.sav';new=pathlib.Path(t)/'QuickSave2.sav'
            p.write_bytes(b'RSG:oldquick')
            put(ship());record(101,99,1);relation=DLL.FHD_Relation(101,99)
            self.assertEqual(save(p),1)
            p.replace(new)
            self.assertEqual(DLL.FHD_BridgeMoveTaggedSlotIfMatch(encoded_path(p),encoded_path(new),tag(new)),1)
            self.assertFalse(pathlib.Path(str(p)+'.fhdnpc').exists())
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(new),1)
            self.assertEqual(DLL.FHD_Relation(101,99),relation)
    def test_migration_refuses_unmoved_sav(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'QuickSave.sav';new=pathlib.Path(t)/'QuickSave2.sav'
            p.write_bytes(b'RSG:oldquick');new.write_bytes(b'RSG:not_related')
            put(ship());self.assertEqual(save(p),1)
            self.assertEqual(DLL.FHD_BridgeMoveTaggedSlotIfMatch(encoded_path(p),encoded_path(new),tag(new)),0)
            self.assertTrue(pathlib.Path(str(p)+'.fhdnpc').exists())
            self.assertFalse(pathlib.Path(str(new)+'.fhdnpc').exists())
    def test_refuses_zero_tag(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'a.sav';p.write_bytes(b'RSG:save')
            put(ship())
            self.assertLess(save(p,savtag=0),0)
    def test_saved_fingerprint_matches_real_file(self):
        with tempfile.TemporaryDirectory() as t:
            p=pathlib.Path(t)/'a.sav';p.write_bytes(b'RSG:123'*100)
            put(ship());self.assertEqual(save(p),1)
            self.assertLess(load(p,savtag=tag(p)^1),0)
if __name__=='__main__':unittest.main(verbosity=2)
