"""0.8 extra adapter checks. Simulated objects, NOT a Windows game test."""
import ctypes as C
import struct
import unittest
from test_live_policy_07 import DLL, Frame, ship, call, after, snap

class Stats(C.Structure):
    _fields_=[('considered',C.c_uint32),('registered',C.c_uint32),('excluded',C.c_uint32),
              ('ranger_choices',C.c_uint32),('ranger_applied',C.c_uint32),
              ('inferred_profit',C.c_uint32),('inferred_loss',C.c_uint32),
              ('confirmed_kills',C.c_uint32),('action_counts',C.c_uint32*10),
              ('categories',C.c_uint32*6),
              ('skip_invalid',C.c_uint32),('skip_provenance',C.c_uint32),
              ('skip_scripts',C.c_uint32),('skip_special',C.c_uint32),
              ('skip_identity',C.c_uint32),('skip_profile',C.c_uint32),
              ('skip_no_action',C.c_uint32),('skip_unsupported',C.c_uint32),
              ('skip_busy',C.c_uint32),('skip_invalid_preferences',C.c_uint32),
              ('skip_no_effect',C.c_uint32),('skip_invalid_pressure',C.c_uint32),
              ('changed_pirates',C.c_uint32),('game_writes_preserved',C.c_uint32),
              ('category_choices',C.c_uint32*6),('category_applied',C.c_uint32*6),('relationship_fields',C.c_uint32*4)]
DLL.FHDNPC_LiveStatsGet.argtypes=[C.POINTER(Stats)]
def stats():
    x=Stats();DLL.FHDNPC_LiveStatsGet(C.byref(x));return x

def pirate(id=7012,pressure=0.5):
    p=ship(id,which=2)
    struct.pack_into('<I',p,0x3f4,0x12345)
    struct.pack_into('<f',p,0x518,pressure)
    return p

class Live08(unittest.TestCase):
    def setUp(self):DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    def test_frame_fits_48_assembly_bytes(self):
        self.assertLessEqual(C.sizeof(Frame),48)
    def test_pirate_pressure_affects_only_one_known_field(self):
        p=pirate();b=snap(p)
        f=call(p,2)
        self.assertEqual(f.valid,1)
        self.assertEqual(f.action,2)
        self.assertEqual(f.pirate_changed,1)
        diffs=[i for i,(a,c) in enumerate(zip(b,snap(p))) if a!=c]
        self.assertTrue(diffs)
        self.assertTrue(set(diffs).issubset(set(range(0x518,0x51c))),diffs)
        self.assertGreater(struct.unpack_from('<f',p,0x518)[0],0.5)
        after(p,f,2)
        self.assertEqual(snap(p),b)
        self.assertEqual(stats().changed_pirates,1)
    def test_game_side_pressure_change_remains(self):
        p=pirate(7013)
        f=call(p,2);self.assertEqual(f.pirate_changed,1)
        struct.pack_into('<f',p,0x518,3.25)
        after(p,f,2)
        self.assertEqual(struct.unpack_from('<f',p,0x518)[0],3.25)
        self.assertEqual(stats().game_writes_preserved,1)
    def test_invalid_pressure_nan_refused(self):
        p=pirate(7014)
        struct.pack_into('<I',p,0x518,0x7fc00001)
        original=snap(p)
        f=call(p,2)
        self.assertEqual(f.valid,1);self.assertEqual(f.pirate_changed,0)
        after(p,f,2);self.assertEqual(snap(p),original)
        self.assertEqual(stats().skip_invalid_pressure,1)
    def test_negative_pressure_refused(self):
        p=pirate(7015,-1.0)
        f=call(p,2)
        self.assertEqual(f.pirate_changed,0)
        self.assertEqual(stats().skip_invalid_pressure,1)
    def test_no_pirate_aggression_when_fleeing(self):
        p=pirate(7016)
        p[0x3b4]=1
        before=snap(p);f=call(p,2)
        self.assertEqual(f.pirate_changed,0)
        after(p,f,2);self.assertEqual(snap(p),before)
    def test_unknown_mod_classification_excluded(self):
        p=pirate(7017)
        struct.pack_into('<I',p,0x0c,0x1000)
        f=call(p,2)
        self.assertEqual(f.valid,0)
        self.assertEqual(stats().skip_provenance,1)
        self.assertEqual(stats().excluded,1)
    def test_dominator_owner_excluded(self):
        p=pirate(7018);p[0x11]=5
        f=call(p,2)
        self.assertEqual(f.valid,0)
        self.assertEqual(stats().skip_provenance,1)
    def test_script_locked_pirate_unchanged(self):
        p=pirate(7019)
        struct.pack_into('<I',p,0x3cc,0x9000)
        before=snap(p);f=call(p,2)
        self.assertEqual(f.valid,0);after(p,f,2)
        self.assertEqual(snap(p),before)
        self.assertEqual(stats().skip_scripts,1)
    def test_other_classes_still_observation_only(self):
        for typ in (1,3,4,5):
            p=ship(8000+typ,which=typ);before=snap(p)
            f=call(p,typ);after(p,f,typ)
            self.assertEqual(snap(p),before)
        self.assertEqual(stats().changed_pirates,0)
    def test_no_forced_pirate_behavior_without_enemy(self):
        p=ship(7020,which=2)
        struct.pack_into('<f',p,0x518,1.0)
        original=snap(p);f=call(p,2)
        self.assertEqual(f.pirate_changed,0)
        after(p,f,2);self.assertEqual(snap(p),original)
    def test_stats_categories_are_updated(self):
        p=pirate(7021);after(p,call(p,2),2)
        q=ship(7022,which=0);after(q,call(q,0),0)
        x=stats()
        self.assertEqual(tuple(x.categories),(1,0,1,0,0,0))
        self.assertEqual(x.registered,2)
if __name__=='__main__':unittest.main()
