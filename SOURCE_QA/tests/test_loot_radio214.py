"""Focused pure-logic tests: EXE-native hooks require an actual game launch."""
import ctypes as C
from pathlib import Path
from test_bridge import DLL
from test_dialogue21 import queue, read_line

ROOT=Path(__file__).resolve().parents[1]
DLL.FHDNPC_LootNextGradeMemory.argtypes=[C.c_uint32,C.c_int32]
DLL.FHDNPC_LootNextGradeMemory.restype=C.c_int32


def grade(v):
    return 4 if v>=80 else 3 if v>=60 else 2 if v>=30 else 1 if v>=10 else 0


def test_one_grade_per_verified_pickup_or_destruction():
    for native in (100, 95, 80, 79, 70, 60, 59, 50, 30, 29, 15, 10):
        memory=0
        last=grade(native)
        for _ in range(5):
            memory=DLL.FHDNPC_LootNextGradeMemory(native, memory)
            modified=max(0,min(100,native+memory))
            assert grade(modified)==max(0,last-1)
            assert modified>=0 # repeated verified conflicts may become hostile
            last=grade(modified)


def test_invalid_native_values_not_affected():
    for native in (0, 1, 9, 101, 255):
        assert DLL.FHDNPC_LootNextGradeMemory(native,0)==1000


def test_modes_apply_immediately_even_after_previous_daily_quota():
    DLL.FHDNPC_DialogueReset()
    DLL.FHD_DialogueContext(5,225)
    assert DLL.FHDNPC_DialogueModeSet(0)==1
    assert not queue(6,2,100,110,1,225,1)
    assert DLL.FHDNPC_DialogueModeSet(1)==1
    assert not queue(6,2,100,110,0,225,2)
    assert queue(1,2,100,110,1,225,3)
    assert read_line()
    assert queue(1,2,100,110,1,225,4)
    assert read_line()
    assert not queue(1,2,100,110,1,225,5) # player mode quota 2
    assert DLL.FHDNPC_DialogueModeSet(2)==1
    assert queue(6,2,101,110,0,225,6)
    assert read_line()
    assert not queue(1,2,102,110,0,225,7) # NPC-NPC loot private
    assert not queue(6,2,103,110,0,225,8,system=6) # other star
    DLL.FHDNPC_DialogueModeSet(0)


def test_info_mirror_native_kind_and_compensation_path_static():
    news=(ROOT/'src/fhd_news.c').read_text()
    grievance=(ROOT/'src/fhd_grievance.c').read_text()
    code=(ROOT/'src/fhd_grievance_stubs.s').read_text()
    assert '#define INFO_KIND 0u' in news
    assert 'FHDNPC_InvokeNews((void *)(image+NEWS_RVA),INFO_KIND,day,info)' in news
    assert 'FHD_DialogueSender()' in news
    assert 'TRY_EXTORT_RVA 0x2d4390u' in grievance
    assert 'process_claim(p,star,day);' in grievance
    assert ('FHDNPC_InvokeMoneyDemand((void*)(g_base+MONEY_RESPONSE_RVA)' in grievance or
            'FHDNPC_InvokeExtort((void*)(g_base+TRY_EXTORT_RVA),issuer,target)' in grievance)
    assert '_FHDNPC_InvokeExtort:' in code
