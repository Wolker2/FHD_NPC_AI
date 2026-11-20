"""Asynchronous game SAVE integration contract: no Windows/game runtime claims."""
import ctypes as C
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from test_bridge import DLL,ship,put,record,encoded_path

KEY=0x8899AABB55667722
for name in ('FHD_BridgePrepareSlot','FHD_BridgeCommitSlot'):
    fn=getattr(DLL,name);fn.argtypes=[C.POINTER(C.c_uint16),C.c_uint64];fn.restype=C.c_int32
DLL.FHD_BridgeAbortSlot.argtypes=[]
DLL.FHD_BridgeAbortSlot.restype=None
DLL.FHD_BridgeMoveSlot.argtypes=[C.POINTER(C.c_uint16),C.POINTER(C.c_uint16)]
DLL.FHD_BridgeMoveSlot.restype=C.c_int32
DLL.FHD_BridgeLoadSlot.argtypes=[C.POINTER(C.c_uint16),C.c_uint64]
DLL.FHD_BridgeLoadSlot.restype=C.c_int32
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_Relation.restype=C.c_int32

def prep(path,key=KEY):return DLL.FHD_BridgePrepareSlot(encoded_path(path),key)
def commit(path,key=KEY):return DLL.FHD_BridgeCommitSlot(encoded_path(path),key)
def load(path,key=KEY):return DLL.FHD_BridgeLoadSlot(encoded_path(path),key)
class Transactions(unittest.TestCase):
    def setUp(self):DLL.FHD_BridgeNewGame()
    def test_capture_before_async_writer_mutates_ai(self):
        with TemporaryDirectory() as td:
            path=Path(td)/'autosave.sav'
            put(ship());record(101,88,1)
            relation_a=DLL.FHD_Relation(101,88)
            self.assertEqual(prep(path),1)
            self.assertFalse(Path(str(path)+'.fhdnpc').exists())
            record(101,88,3)
            relation_b=DLL.FHD_Relation(101,88)
            self.assertNotEqual(relation_a,relation_b)
            self.assertEqual(commit(path),1)
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(path),1)
            self.assertEqual(DLL.FHD_Relation(101,88),relation_a)
    def test_writer_failure_does_not_overwrite_old_sidecar(self):
        with TemporaryDirectory() as td:
            p=Path(td)/'game.sav';put(ship());record(101,88,1)
            self.assertEqual(prep(p),1);self.assertEqual(commit(p),1)
            initial=Path(str(p)+'.fhdnpc').read_bytes()
            record(101,88,3)
            self.assertEqual(prep(p),1)
            DLL.FHD_BridgeAbortSlot()
            self.assertEqual(Path(str(p)+'.fhdnpc').read_bytes(),initial)
            self.assertFalse(Path(str(p)+'.fhdnpc.tmp').exists())
    def test_wrong_slot_cannot_commit(self):
        with TemporaryDirectory() as td:
            a=Path(td)/'one.sav';b=Path(td)/'two.sav'
            put(ship());self.assertEqual(prep(a),1)
            self.assertEqual(commit(b),0)
            self.assertEqual(commit(a,key=KEY+1),0)
            self.assertEqual(commit(a),1)
    def test_new_game_aborts_pending(self):
        with TemporaryDirectory() as td:
            p=Path(td)/'s.sav';put(ship())
            self.assertEqual(prep(p),1)
            DLL.FHD_BridgeNewGame()
            self.assertFalse(Path(str(p)+'.fhdnpc.tmp').exists())
            self.assertEqual(commit(p),0)
    def test_failed_path_rejected_without_creating_sidecar(self):
        with TemporaryDirectory() as td:
            p=Path(td)/'unavailable'/'s.sav';put(ship())
            self.assertLess(prep(p),0)
            self.assertEqual(commit(p),0)
    def test_verified_quick_save_rotation_moves_memory_with_sav(self):
        with TemporaryDirectory() as td:
            a=Path(td)/'quick.sav';b=Path(td)/'quick2.sav'
            put(ship());record(101,88,1)
            initial=DLL.FHD_Relation(101,88)
            self.assertEqual(prep(a),1);self.assertEqual(commit(a),1)
            # The original game moved the SAV already; mirror ONLY on success.
            self.assertEqual(DLL.FHD_BridgeMoveSlot(encoded_path(a),encoded_path(b)),1)
            self.assertFalse(Path(str(a)+'.fhdnpc').exists())
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(b),1)
            self.assertEqual(DLL.FHD_Relation(101,88),initial)
    def test_missing_rotated_slot_clears_stale_target(self):
        with TemporaryDirectory() as td:
            a=Path(td)/'quick.sav';b=Path(td)/'quick2.sav'
            put(ship());self.assertEqual(prep(b),1);self.assertEqual(commit(b),1)
            self.assertEqual(DLL.FHD_BridgeMoveSlot(encoded_path(a),encoded_path(b)),0)
            self.assertFalse(Path(str(b)+'.fhdnpc').exists())
    def test_new_prepare_aborts_previous_pending(self):
        with TemporaryDirectory() as td:
            a=Path(td)/'a.sav';b=Path(td)/'b.sav';put(ship())
            self.assertEqual(prep(a),1);self.assertEqual(prep(b),1)
            self.assertFalse(Path(str(a)+'.fhdnpc.tmp').exists())
            self.assertEqual(commit(a),0);self.assertEqual(commit(b),1)
if __name__=='__main__':unittest.main(verbosity=2)
