import ctypes as C
import os
import pathlib
import time
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
LIB=C.CDLL(str(ROOT/'build/libfhd_npc_bridge_test.so'))

class Profile(C.Structure):
    _pack_=1
    _fields_=[('id',C.c_uint32),('character',C.c_uint8),('role',C.c_uint8),
              ('traits',C.c_uint8*10),('profession',C.c_uint8*3),
              ('reserved',C.c_uint8),('events',C.c_uint32)]
class Situation(C.Structure):
    _pack_=1
    _fields_=[('target_id',C.c_uint32),('danger',C.c_uint8),
              ('expected_profit',C.c_uint8),('hull_damage',C.c_uint8),
              ('relative_enemy_power',C.c_uint8),('combat_available',C.c_uint8),
              ('piracy_available',C.c_uint8),('trading_available',C.c_uint8),
              ('repair_available',C.c_uint8),('flee_available',C.c_uint8),
              ('ally_in_danger',C.c_uint8),('explore_available',C.c_uint8),
              ('diplomacy_available',C.c_uint8),('mission_available',C.c_uint8),
              ('reserved',C.c_uint8*3)]
class Decision(C.Structure):
    _pack_=1
    _fields_=[('action',C.c_uint32),('score',C.c_int32),('relation_to_target',C.c_int32)]

LIB.FHD_Version.restype=C.c_uint32
LIB.FHD_Upsert.argtypes=[C.c_uint32,C.c_uint32,C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
LIB.FHD_Upsert.restype=C.c_int32
LIB.FHD_ProfileOf.argtypes=[C.c_uint32,C.POINTER(Profile)]; LIB.FHD_ProfileOf.restype=C.c_int32
LIB.FHD_Record.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]; LIB.FHD_Record.restype=C.c_int32
LIB.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]; LIB.FHD_Relation.restype=C.c_int32
LIB.FHD_Choose.argtypes=[C.c_uint32,C.POINTER(Situation),C.POINTER(Decision)]; LIB.FHD_Choose.restype=C.c_int32
LIB.FHD_Snapshot.argtypes=[C.c_void_p,C.c_uint32];LIB.FHD_Snapshot.restype=C.c_int32
LIB.FHD_Restore.argtypes=[C.c_void_p,C.c_uint32];LIB.FHD_Restore.restype=C.c_int32
LIB.FHD_AdvanceDay.argtypes=[C.c_uint32]
LIB.FHD_Count.restype=C.c_uint32
LIB.FHD_Forget.argtypes=[C.c_uint32];LIB.FHD_Forget.restype=C.c_int32

def put(id, character=0, role=1, native=1, flags=0, p=0,t=0,m=0,seed=None):
    return LIB.FHD_Upsert(id,id*17 if seed is None else seed,character,role,native,flags,p,t,m)
def profile(id):
    p=Profile()
    return p if LIB.FHD_ProfileOf(id,C.byref(p)) else None
def situation(**values):
    x=Situation()
    for k,v in values.items():setattr(x,k,v)
    return x
def choose(id,s):
    x=Decision();result=LIB.FHD_Choose(id,C.byref(s),C.byref(x))
    return result,x

def snapshot():
    size=LIB.FHD_Snapshot(None,0)
    arr=(C.c_ubyte*size)()
    assert LIB.FHD_Snapshot(arr,size)==size
    return bytes(arr)

def restore(b):
    arr=(C.c_ubyte*len(b)).from_buffer_copy(b)
    return LIB.FHD_Restore(arr,len(b))

class CoreTests(unittest.TestCase):
    def setUp(self):LIB.FHD_Reset()
    def test_abi(self):self.assertEqual(LIB.FHD_Version(),3)
    def test_explicit_characters_all_11(self):
        for ch in range(11):
            self.assertEqual(put(ch+1,character=ch),1)
            prof=profile(ch+1)
            self.assertEqual(prof.character,ch)
            self.assertEqual(sum(prof.profession),100)
        self.assertEqual(profile(1).profession[0],100)
        self.assertEqual(profile(5).profession[1],100)
        self.assertEqual(profile(11).profession[2],100)
    def test_auto_character_selection(self):
        put(1,character=-1,p=100,t=0,m=0)
        put(2,character=-1,p=0,t=100,m=0)
        put(3,character=-1,p=0,t=0,m=100)
        self.assertEqual([profile(x).character for x in [1,2,3]],[0,4,10])
    def test_same_focus_distinct_personality(self):
        put(1,9); put(2,9)
        self.assertNotEqual(list(profile(1).traits),list(profile(2).traits))
        a=list(profile(1).traits)
        LIB.FHD_Reset();put(1,9)
        self.assertEqual(list(profile(1).traits),a)
    def test_no_rng_changes_on_upsert(self):
        put(1,9);a=bytes(profile(1))
        put(1,9)
        self.assertEqual(bytes(profile(1)),a)
    def test_dominators_and_klissans_excluded(self):
        self.assertEqual(put(1,native=0),0)
        self.assertEqual(put(2,native=1,flags=2),0)
        self.assertEqual(put(3,native=1,flags=4),0)
        self.assertEqual(put(4,native=1,flags=1),0)
        self.assertEqual(put(5,native=12),0)
        self.assertEqual(put(6,native=47),0)
        self.assertEqual(LIB.FHD_Count(),0)
    def test_modded_ship_included(self):
        self.assertEqual(put(12,4,role=127,native=47,flags=32),1)
        self.assertEqual(profile(12).role,127)
    def test_quest_lock_opt_out(self):
        self.assertEqual(put(42,flags=8),0)
        self.assertEqual(put(43,flags=16),0)
        self.assertEqual(LIB.FHD_Count(),0)
    def test_remap_to_klissan_disables_existing(self):
        put(42,8)
        self.assertEqual(put(42,8,flags=2),0)
        self.assertIsNone(profile(42))
        self.assertEqual(choose(42,situation(trading_available=1))[0],0)
    def test_relationship_friend_betray(self):
        put(7,10)
        self.assertEqual(LIB.FHD_Record(7,21,1,8,2),1)
        self.assertGreater(LIB.FHD_Relation(7,21),0)
        self.assertEqual(LIB.FHD_Record(7,21,3,8,3),1)
        self.assertLess(LIB.FHD_Relation(7,21),0)
        self.assertEqual(profile(7).events,2)
    def test_relationship_bounds_and_memory_limit(self):
        put(7,10)
        for d in range(200):LIB.FHD_Record(7,21,3,10,d)
        self.assertEqual(LIB.FHD_Relation(7,21),-100)
        for other in range(100,200):LIB.FHD_Record(7,other,1,1,300)
        self.assertLessEqual(len(snapshot()),30000)
    def test_routine_victory_preserves_personality(self):
        put(7,10)
        old=profile(7).traits[0]
        LIB.FHD_Record(7,0,4,1,1)
        self.assertEqual(profile(7).traits[0],old)
    def test_diplomat_and_mission_supported(self):
        put(7,8,role=5)
        result,decision=choose(7,situation(diplomacy_available=1))
        self.assertEqual((result,decision.action),(1,8))
        result,decision=choose(7,situation(mission_available=1))
        self.assertEqual((result,decision.action),(1,9))
    def test_profession_drives_action(self):
        put(1,0); put(2,4);put(3,10)
        s=situation(trading_available=1,piracy_available=1,combat_available=1,
                    relative_enemy_power=35,expected_profit=80)
        actions=[choose(x,s)[1].action for x in (1,2,3)]
        self.assertEqual(actions,[2,1,3])
    def test_repair_and_flee(self):
        put(7,10)
        self.assertEqual(choose(7,situation(repair_available=1,hull_damage=80))[1].action,4)
        self.assertEqual(choose(7,situation(flee_available=1,danger=80))[1].action,5)
    def test_no_action_without_options(self):
        put(7,10)
        self.assertEqual(choose(7,situation())[1].action,0)
    def test_snapshot_roundtrip(self):
        put(31,5);LIB.FHD_Record(31,8,3,10,4)
        before=bytes(profile(31));relation=LIB.FHD_Relation(31,8)
        saved=snapshot();LIB.FHD_Reset();self.assertEqual(LIB.FHD_Count(),0)
        self.assertEqual(restore(saved),1)
        self.assertEqual(bytes(profile(31)),before)
        self.assertEqual(LIB.FHD_Relation(31,8),relation)
    def test_corrupt_snapshot_is_transactional(self):
        put(31,5);saved=bytearray(snapshot())
        put(32,5);count=LIB.FHD_Count()
        saved[-1]^=0x20
        self.assertLess(restore(bytes(saved)),0)
        self.assertEqual(LIB.FHD_Count(),count)
    def test_truncated_snapshot_is_transactional(self):
        put(1,1);saved=snapshot();put(2,2)
        self.assertLess(restore(saved[:-1]),0)
        self.assertEqual(LIB.FHD_Count(),2)
    def test_weak_relations_fade(self):
        put(1,4);LIB.FHD_Record(1,3,1,2,1)
        before=LIB.FHD_Relation(1,3)
        LIB.FHD_AdvanceDay(31)
        self.assertLess(LIB.FHD_Relation(1,3),before)
    def test_8192_agents_capacity(self):
        t=time.perf_counter()
        for id in range(1,8193):self.assertEqual(put(id,id%11,role=127,native=17,flags=32),1)
        self.assertEqual(LIB.FHD_Count(),8192)
        self.assertEqual(put(8193),-2)
        self.assertTrue(len(snapshot())>1000000)
        self.assertLess(time.perf_counter()-t,6.0)
    def test_ship_id_reuse_clears_memory(self):
        put(17,4,seed=100)
        LIB.FHD_Record(17,22,3,10,1)
        self.assertNotEqual(LIB.FHD_Relation(17,22),0)
        put(17,4,seed=200)
        self.assertEqual(LIB.FHD_Relation(17,22),0)
    def test_ship_lifecycle_allows_replacement(self):
        put(17,4)
        self.assertEqual(LIB.FHD_Forget(17),1)
        self.assertEqual(LIB.FHD_Forget(17),0)
        self.assertEqual(LIB.FHD_Count(),0)
        put(17,4)
        self.assertEqual(LIB.FHD_Count(),1)
    def test_long_term_churn(self):
        for i in range(1,12001):
            self.assertEqual(put(i,4),1)
            self.assertEqual(LIB.FHD_Forget(i),1)
        self.assertEqual(LIB.FHD_Count(),0)
        self.assertEqual(put(60000,4),1)
        self.assertEqual(restore(snapshot()),1)
    def test_reversible_role_override(self):
        put(5,8,role=1);before=profile(5).traits[:]
        put(5,8,role=7)
        self.assertEqual(profile(5).role,7)
        self.assertEqual(list(profile(5).traits),list(before))

if __name__=='__main__':unittest.main(verbosity=2)
