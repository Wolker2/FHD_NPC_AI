"""Linux simulated game ABI contract tests. They are NOT a Windows/game launch."""
import ctypes as C
import tempfile
import pathlib
import struct
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
DLL=C.CDLL(str(ROOT/'build/libfhd_npc_bridge_test.so'))

class Descriptor(C.Structure):
    _pack_=1
    _fields_=[('id',C.c_uint32),('seed',C.c_uint32),('native_type',C.c_uint32),('flags',C.c_uint32),
              ('origin',C.c_uint32),('owner',C.c_uint32),('role',C.c_uint32),
              ('character',C.c_int32),('pirate',C.c_uint32),('trade',C.c_uint32),('military',C.c_uint32)]
class Situation(C.Structure):
    _pack_=1
    _fields_=[('target_id',C.c_uint32),('danger',C.c_uint8),('expected_profit',C.c_uint8),
              ('hull_damage',C.c_uint8),('relative_enemy_power',C.c_uint8),('combat_available',C.c_uint8),
              ('piracy_available',C.c_uint8),('trading_available',C.c_uint8),
              ('repair_available',C.c_uint8),('flee_available',C.c_uint8),('ally_in_danger',C.c_uint8),
              ('explore_available',C.c_uint8),('diplomacy_available',C.c_uint8),('mission_available',C.c_uint8),
              ('reserved',C.c_uint8*3)]
class Context(C.Structure):
    _pack_=1
    _fields_=[('ship',Descriptor),('situation',Situation)]
class Decision(C.Structure):
    _pack_=1
    _fields_=[('action',C.c_uint32),('score',C.c_int32),('relation',C.c_int32)]
class Event(C.Structure):
    _pack_=1
    _fields_=[('observer_id',C.c_uint32),('other_id',C.c_uint32),('kind',C.c_uint32),
              ('magnitude',C.c_uint32),('day',C.c_uint32)]
CALLBACK=C.CFUNCTYPE(C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_void_p)
DLL.FHD_BridgeVersion.restype=C.c_uint32
DLL.FHD_BridgeManagedCount.restype=C.c_uint32
DLL.FHD_BridgeUpsert.argtypes=[C.POINTER(Descriptor)]; DLL.FHD_BridgeUpsert.restype=C.c_int32
DLL.FHD_BridgeForget.argtypes=[C.c_uint32];DLL.FHD_BridgeForget.restype=C.c_int32
DLL.FHD_BridgeEvent.argtypes=[C.POINTER(Event)];DLL.FHD_BridgeEvent.restype=C.c_int32
DLL.FHD_BridgeDay.argtypes=[C.c_uint32]
DLL.FHD_BridgeDecide.argtypes=[C.POINTER(Context),CALLBACK,C.c_void_p,C.POINTER(Decision)]
DLL.FHD_BridgeDecide.restype=C.c_int32
DLL.FHD_BridgeSaveSlot.argtypes=[C.POINTER(C.c_uint16),C.c_uint64];DLL.FHD_BridgeSaveSlot.restype=C.c_int32
DLL.FHD_BridgeLoadSlot.argtypes=[C.POINTER(C.c_uint16),C.c_uint64];DLL.FHD_BridgeLoadSlot.restype=C.c_int32
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]; DLL.FHD_Relation.restype=C.c_int32
DLL.FHD_Restore.argtypes=[C.c_void_p,C.c_uint32]; DLL.FHD_Restore.restype=C.c_int32
DLL.FHD_Snapshot.argtypes=[C.c_void_p,C.c_uint32]; DLL.FHD_Snapshot.restype=C.c_int32
DLL.FHD_ProfileOf.argtypes=[C.c_uint32,C.c_void_p];DLL.FHD_ProfileOf.restype=C.c_int32

# Game-mapped native types: dominator 0, ranger 1, transport 2, pirate 3, warrior 4,
# tranclucator 5, stations 6..11, dominator station 12, custom station 13.
def ship(id=101,typ=1,origin=1,owner=1,role=1,char=10,flags=0,seed=123456):
    return Descriptor(id,seed,typ,flags,origin,owner,role,char,0,0,100)
def context(ship_record,**fields):
    s=Situation()
    for field,val in fields.items():setattr(s,field,val)
    return Context(ship_record,s)
def put(d):return DLL.FHD_BridgeUpsert(C.byref(d))
def record(observer,other,kind,magnitude=4,day=100):
    e=Event(observer,other,kind,magnitude,day)
    return DLL.FHD_BridgeEvent(C.byref(e))
def encoded_path(path):
    text=str(path)
    return (C.c_uint16*(len(text)+1))(*map(ord,text),0)
def save(path,key=0x2222222222222222):return DLL.FHD_BridgeSaveSlot(encoded_path(path),key)
def load(path,key=0x2222222222222222):return DLL.FHD_BridgeLoadSlot(encoded_path(path),key)
def snapshot():
    size=DLL.FHD_Snapshot(None,0)
    b=(C.c_ubyte*size)()
    assert DLL.FHD_Snapshot(b,size)==size
    return bytes(b)

class BridgeTests(unittest.TestCase):
    def setUp(self):DLL.FHD_BridgeNewGame()
    def test_abi_v5(self):self.assertEqual(DLL.FHD_BridgeVersion(),5)
    def test_vanilla_ranger(self):self.assertEqual(put(ship()),1)
    def test_all_vanilla_movable_types(self):
        for typ in (1,2,3,4,6,7,8,9,10,11):self.assertEqual(put(ship(id=typ+100,typ=typ)),1)
        self.assertEqual(DLL.FHD_BridgeManagedCount(),10)
    def test_tranclucator_is_excluded(self):self.assertEqual(put(ship(typ=5)),0)
    def test_dominator_ship(self):self.assertEqual(put(ship(typ=0)),0)
    def test_dominator_station(self):self.assertEqual(put(ship(typ=12)),0)
    def test_unknown_mod_origin(self):self.assertEqual(put(ship(origin=0)),0)
    def test_unclassified_mod_using_vanilla_type(self):self.assertEqual(put(ship(origin=0,typ=1)),0)
    def test_mod_verified_same_type_allowed(self):self.assertEqual(put(ship(origin=2,typ=1)),1)
    def test_mod_verified_custom_type(self):self.assertEqual(put(ship(origin=2,typ=47)),1)
    def test_unverified_custom_station(self):self.assertEqual(put(ship(typ=13)),0)
    def test_verified_custom_station(self):self.assertEqual(put(ship(typ=13,origin=2)),1)
    def test_unknown_owner(self):self.assertEqual(put(ship(owner=0)),0)
    def test_dominator_owner_with_normal_type(self):self.assertEqual(put(ship(owner=3,typ=1)),0)
    def test_klissan_owner_with_normal_type(self):self.assertEqual(put(ship(owner=4,typ=1)),0)
    def test_klissan_mod_origin(self):self.assertEqual(put(ship(origin=4,typ=47)),0)
    def test_dominator_mod_origin(self):self.assertEqual(put(ship(origin=3,typ=37)),0)
    def test_klissan_reclassification_purges_agent(self):
        self.assertEqual(put(ship()),1)
        record(101,99,3)
        self.assertEqual(put(ship(origin=4)),0)
        self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
        self.assertEqual(DLL.FHD_Relation(101,99),0)
    def test_script_lock(self):self.assertEqual(put(ship(flags=8)),0)
    def test_story_lock(self):self.assertEqual(put(ship(flags=16)),0)
    def test_player(self):self.assertEqual(put(ship(flags=1)),0)
    def test_unknown_origin_deactivates_old(self):
        self.assertEqual(put(ship()),1)
        self.assertEqual(put(ship(origin=0)),0)
        self.assertEqual(record(101,202,1),0)
    def test_invalid_descriptor(self):
        self.assertLess(put(ship(id=0)),0)
        self.assertLess(put(ship(origin=99)),0)
        self.assertLess(put(ship(owner=99)),0)
    def test_event_records_experiences(self):
        self.assertEqual(put(ship()),1)
        self.assertEqual(record(101,900,1),1)
        self.assertGreater(DLL.FHD_Relation(101,900),0)
    def test_event_target_need_not_be_managed(self):
        self.assertEqual(put(ship()),1)
        self.assertEqual(record(101,909,3),1)
        self.assertLess(DLL.FHD_Relation(101,909),0)
    def test_event_exemption(self):
        self.assertEqual(put(ship(owner=3)),0)
        self.assertEqual(record(101,900,3),0)
    def test_event_invalid(self):
        self.assertLess(record(101,900,900),0)
    def test_host_executor_success(self):
        seen=[]
        @CALLBACK
        def execute(id,action,target,_):seen.append((id,action,target));return 1
        ctx=context(ship(),combat_available=1,target_id=78)
        d=Decision()
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(ctx),execute,None,C.byref(d)),1)
        self.assertEqual(seen,[(101,3,78)])
        self.assertEqual(d.action,3)
    def test_host_executor_failed_passes_to_vanilla(self):
        @CALLBACK
        def execute(*_):return 0
        d=Decision()
        ctx=context(ship(),combat_available=1)
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(ctx),execute,None,C.byref(d)),0)
        self.assertEqual(d.action,0)
    def test_no_available_action_falls_back(self):
        calls=[]
        @CALLBACK
        def execute(*_):calls.append(1);return 1
        d=Decision()
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(context(ship())),execute,None,C.byref(d)),0)
        self.assertEqual(calls,[])
    def test_reentrant_decision_is_rejected(self):
        called=[]
        @CALLBACK
        def execute(*_):
            child=Decision()
            nested=DLL.FHD_BridgeDecide(C.byref(context(ship())),execute,None,C.byref(child))
            called.append(nested)
            return 1
        d=Decision();ctx=context(ship(),combat_available=1)
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(ctx),execute,None,C.byref(d)),1)
        self.assertEqual(called,[0])
    def test_host_cannot_execute_excluded_ship(self):
        called=[]
        @CALLBACK
        def execute(*_):called.append(1);return 1
        d=Decision();ctx=context(ship(origin=4),combat_available=1)
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(ctx),execute,None,C.byref(d)),0)
        self.assertFalse(called)
    def test_event_after_decline_unchanged(self):
        @CALLBACK
        def execute(*_):return 0
        d=Decision()
        ctx=context(ship(),combat_available=1,target_id=90)
        self.assertEqual(DLL.FHD_BridgeDecide(C.byref(ctx),execute,None,C.byref(d)),0)
        self.assertEqual(DLL.FHD_Relation(101,90),0)
    def test_sidecar_saved_and_loaded(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'slot1.sav'
            self.assertEqual(put(ship()),1);record(101,7,3)
            relation=DLL.FHD_Relation(101,7)
            self.assertEqual(save(path),1)
            self.assertTrue(pathlib.Path(str(path)+'.fhdnpc').exists())
            self.assertFalse(pathlib.Path(str(path)+'.fhdnpc.tmp').exists())
            self.assertFalse(path.exists())  # we NEVER modify game SAV
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(path),1)
            self.assertEqual(DLL.FHD_Relation(101,7),relation)
    def test_two_slots_independent(self):
        with tempfile.TemporaryDirectory() as td:
            a=pathlib.Path(td)/'a.sav';b=pathlib.Path(td)/'b.sav'
            put(ship());record(101,7,1)
            happy=DLL.FHD_Relation(101,7)
            self.assertEqual(save(a),1)
            record(101,7,3)
            angry=DLL.FHD_Relation(101,7)
            self.assertEqual(save(b),1)
            self.assertEqual(load(a),1);self.assertEqual(DLL.FHD_Relation(101,7),happy)
            self.assertEqual(load(b),1);self.assertEqual(DLL.FHD_Relation(101,7),angry)
    def test_missing_sidecar_clears_memory(self):
        with tempfile.TemporaryDirectory() as td:
            put(ship());record(101,7,3)
            self.assertLess(load(pathlib.Path(td)/'missing.sav'),0)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_wrong_galaxy_key_clears_memory(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'a.sav'
            put(ship());record(101,7,3);self.assertEqual(save(path),1)
            self.assertLess(load(path,key=7),0)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_corrupt_sidecar_clears_memory(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'a.sav'
            put(ship());record(101,7,3);self.assertEqual(save(path),1)
            f=pathlib.Path(str(path)+'.fhdnpc')
            data=bytearray(f.read_bytes());data[-1]^=1;f.write_bytes(data)
            self.assertLess(load(path),0)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_truncated_sidecar_clears_memory(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'a.sav'
            put(ship());self.assertEqual(save(path),1)
            f=pathlib.Path(str(path)+'.fhdnpc');f.write_bytes(f.read_bytes()[:13])
            self.assertLess(load(path),0)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_reload_new_game_does_not_retain_memory(self):
        put(ship());record(101,7,3)
        DLL.FHD_BridgeNewGame()
        self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_guard_prevents_self_relation(self):
        put(ship());record(101,101,3)
        self.assertEqual(DLL.FHD_Relation(101,101),0)
    def test_save_overwrites_same_slot(self):
        with tempfile.TemporaryDirectory() as td:
            p=pathlib.Path(td)/'a.sav';put(ship())
            self.assertEqual(save(p),1)
            record(101,44,3)
            self.assertEqual(save(p),1)
            self.assertEqual(load(p),1)
            self.assertLess(DLL.FHD_Relation(101,44),0)
    def test_large_galaxy_sidecar_8192(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'large.sav'
            for i in range(1,8193):
                typ=(1,2,3,4,6,7,8,9,10,11)[i%10]
                d=ship(id=i,seed=i*771,typ=typ,origin=1)
                self.assertEqual(put(d),1)
                if i%101==0:record(i,0x100000+i,3)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),8192)
            self.assertEqual(save(path),1)
            sidecar=pathlib.Path(str(path)+'.fhdnpc')
            self.assertLess(sidecar.stat().st_size,4194304)
            DLL.FHD_BridgeNewGame()
            self.assertEqual(load(path),1)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),8192)
            self.assertLess(DLL.FHD_Relation(101,0x100000+101),0)
    def test_save_too_long_path_fails_safely(self):
        put(ship())
        self.assertLess(save('a'*2100),0)
        self.assertEqual(DLL.FHD_BridgeManagedCount(),1)
    def test_save_load_zero_agent_slot(self):
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'empty.sav'
            self.assertEqual(save(path),1)
            self.assertEqual(load(path),1)
            self.assertEqual(DLL.FHD_BridgeManagedCount(),0)
    def test_snapshot_duplicate_id_rejected_without_mutation(self):
        # Use 2 profiles, edit second id to first and repair FNV checksum.
        put(ship(id=101));put(ship(id=102))
        before=DLL.FHD_BridgeManagedCount()
        data=bytearray(snapshot())
        header=28
        recordsize=struct.unpack_from('<I',data,20)[0]
        # Snapshot header is 28 bytes: magic(8), abi/count/day/size/hash(20)
        self.assertGreater(recordsize,100)
        first=struct.unpack_from('<I',data,header)[0]
        second_off=header+recordsize
        struct.pack_into('<I',data,second_off,first)
        # Also align profile.id to hide the accidental duplicate from naive validation.
        struct.pack_into('<I',data,second_off+16,first)
        h=2166136261
        for byte in data[header:]:h=((h^byte)*16777619)&0xffffffff
        struct.pack_into('<I',data,24,h)
        buffer=(C.c_ubyte*len(data)).from_buffer_copy(data)
        self.assertLess(DLL.FHD_Restore(buffer,len(data)),0)
        self.assertEqual(DLL.FHD_BridgeManagedCount(),before)

if __name__=='__main__':unittest.main(verbosity=2)
