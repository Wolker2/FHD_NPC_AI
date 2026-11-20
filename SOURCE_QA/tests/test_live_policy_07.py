"""0.7.0 live adapter regression tests, using exact 2026 TShip/ TRanger offsets.
These are fixture tests, not a Windows game launch.
"""
import ctypes as C
import struct
import unittest
from pathlib import Path

DLL=C.CDLL(str(Path(__file__).resolve().parents[1]/'build/libfhd_npc_bridge_test.so'))
class Frame(C.Structure):
    _pack_=1
    _fields_=[('id',C.c_uint32),('seed',C.c_uint32),('day',C.c_uint32),
              ('money',C.c_int32),('kills',C.c_int32),('action',C.c_uint32),('valid',C.c_uint32),
              ('old_career',C.c_uint8),('old_aggression',C.c_uint8),
              ('used_career',C.c_uint8),('used_aggression',C.c_uint8),
              ('changed',C.c_uint8),('pirate_changed',C.c_uint8),('reserved',C.c_uint8*2),
              ('old_pressure_bits',C.c_uint32),('used_pressure_bits',C.c_uint32)]
class Profile(C.Structure):
    _pack_=1
    _fields_=[('id',C.c_uint32),('character',C.c_uint8),('role',C.c_uint8),('traits',C.c_uint8*10),
              ('profession',C.c_uint8*3),('reserved',C.c_uint8),('events',C.c_uint32)]
DLL.FHDNPC_LiveBefore.argtypes=[C.POINTER(C.c_uint8),C.c_uint32,C.POINTER(Frame)]
DLL.FHDNPC_LiveAfter.argtypes=DLL.FHDNPC_LiveBefore.argtypes
DLL.FHD_ProfileOf.argtypes=[C.c_uint32,C.POINTER(Profile)]
DLL.FHD_ProfileOf.restype=C.c_int32
DLL.FHD_Record.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
DLL.FHDNPC_LiveReset.argtypes=[]
DLL.FHD_BridgeNewGame.argtypes=[]

def ship(n,owner=2,which=0,career=(5,5,90),money=1000,day=100):
    a=(C.c_uint8*0x600)()
    a[0x10]=which+1 if which<5 else 6
    a[0x11]=owner
    struct.pack_into('<I',a,4,n)
    struct.pack_into('<I',a,0xe0,123456+n)
    struct.pack_into('<I',a,0xec,day)
    struct.pack_into('<i',a,0xf0,money)
    struct.pack_into('<i',a,0x4d4,0)
    a[0x513:0x516]=career
    a[0x51c]=0;a[0x51d]=40
    return a

def snap(a):return bytes(a)
def call(a,w=0):
    frame=Frame()
    DLL.FHDNPC_LiveBefore(a,w,C.byref(frame))
    return frame

def after(a,f,w=0):DLL.FHDNPC_LiveAfter(a,w,C.byref(f))
def profile(id):
    p=Profile();assert DLL.FHD_ProfileOf(id,C.byref(p))==1;return p

class LiveTests(unittest.TestCase):
    def setUp(self):DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    def test_profile_from_real_career_values(self):
        for id, weights, character in [(101,(0,100,0),0),(102,(100,0,0),4),(103,(0,0,100),10)]:
            a=ship(id,career=weights); f=call(a);after(a,f)
            self.assertEqual(profile(id).character,character)
    def test_different_personality_same_career(self):
        a=ship(201);b=ship(202)
        after(a,call(a));after(b,call(b))
        self.assertNotEqual(tuple(profile(201).traits),tuple(profile(202).traits))
    def test_ranger_temporarily_changes_game_fields(self):
        a=ship(301,career=(0,90,10));struct.pack_into('<I',a,0x3f4,0x12345678);before=snap(a)
        f=call(a)
        self.assertEqual(f.valid,1);self.assertEqual(f.action,2)
        self.assertEqual(f.changed,1)
        changed=[i for i,(x,y) in enumerate(zip(before,snap(a))) if x!=y]
        self.assertTrue(set(changed).issubset({0x51c,0x51d}),changed)
        after(a,f);self.assertEqual(snap(a),before)
    def test_trade_focus_changes_aggression(self):
        a=ship(302,career=(100,0,0));before=snap(a)
        f=call(a);self.assertEqual(f.action,1);self.assertTrue(f.changed)
        after(a,f);self.assertEqual(snap(a),before)
    def test_vanilla_normal_ship_keeps_every_byte(self):
        a=ship(303,which=1);before=snap(a)
        f=call(a,1);self.assertEqual(f.valid,1)
        after(a,f,1);self.assertEqual(snap(a),before)
    def test_dominator_and_unsupported_owner_untouched(self):
        for i,owner in enumerate([5,6,8,255]):
            a=ship(401+i,owner=owner);before=snap(a)
            f=call(a);after(a,f)
            self.assertEqual(f.valid,0);self.assertEqual(snap(a),before)
    def test_unverified_mod_override_untouched(self):
        a=ship(501);struct.pack_into('<I',a,0x0c,0x11223344);before=snap(a)
        f=call(a);after(a,f)
        self.assertEqual(f.valid,0);self.assertEqual(snap(a),before)
    def test_script_locked_untouched(self):
        a=ship(601);struct.pack_into('<I',a,0x3cc,1000);before=snap(a)
        f=call(a);after(a,f)
        self.assertEqual(f.valid,0);self.assertEqual(snap(a),before)
    def test_absolute_order_untouched(self):
        a=ship(701);a[0x43c]=1;before=snap(a)
        f=call(a);after(a,f)
        self.assertEqual(f.valid,0);self.assertEqual(snap(a),before)
    def test_partnership_skips_change(self):
        a=ship(801);struct.pack_into('<I',a,0x3fc,100);before=snap(a)
        f=call(a);after(a,f)
        self.assertEqual(f.valid,1);self.assertEqual(f.changed,0);self.assertEqual(snap(a),before)
    def test_ranger_game_update_not_overwritten(self):
        a=ship(901);f=call(a)
        a[0x51d]=3 if f.used_aggression!=3 else 2
        after(a,f);self.assertNotEqual(a[0x51d],f.old_aggression)
    def test_money_event_not_called_trade(self):
        a=ship(1001);f=call(a)
        struct.pack_into('<i',a,0xf0,2000)
        after(a,f);self.assertEqual(profile(1001).events,0)
    def test_duplicate_money_same_day_not_recorded(self):
        a=ship(1101);f=call(a);struct.pack_into('<i',a,0xf0,2000);after(a,f)
        f=call(a);struct.pack_into('<i',a,0xf0,4000);after(a,f)
        self.assertEqual(profile(1101).events,0)
    def test_kill_increments_memory(self):
        a=ship(1201);f=call(a);struct.pack_into('<i',a,0x4d4,1);after(a,f)
        self.assertEqual(profile(1201).events,1)
    def test_kill_between_logic_calls_is_detected(self):
        a=ship(1251);after(a,call(a))
        struct.pack_into('<i',a,0x4d4,1)
        f=call(a);after(a,f)
        self.assertEqual(profile(1251).events,1)
    def test_unclassified_balance_change_does_not_count_as_profit(self):
        a=ship(1252);after(a,call(a))
        struct.pack_into('<i',a,0xf0,3000)
        f=call(a);after(a,f)
        self.assertEqual(profile(1252).events,0)
    def test_new_game_resets_memory(self):
        a=ship(1301);after(a,call(a));self.assertEqual(DLL.FHD_Count(),1)
        DLL.FHD_BridgeNewGame();self.assertEqual(DLL.FHD_Count(),0)
    def test_eleven_characters_generated(self):
        # Core is able to handle all 11 manually supplied character types.
        DLL.FHD_Upsert.argtypes=[C.c_uint32,C.c_uint32,C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
        for k in range(11):
            self.assertEqual(DLL.FHD_Upsert(1401+k,k+100,k,1,1,0,33,33,34),1)
            self.assertEqual(profile(1401+k).character,k)

if __name__=='__main__':unittest.main()
