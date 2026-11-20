"""World reports and migration checks for the living economy build."""
import ctypes as C
import os
from pathlib import Path
import struct
import pytest

from test_bridge import DLL, snapshot
from test_foundation18 import restore


def test_19_snapshot_migrates_without_inventing_reserved_credits():
    data = (Path(__file__).parent / 'fixtures' / 'fhd_npc_ai_1_9_duties.bin').read_bytes()
    assert restore(data) == 3
    assert DLL.FHD_Relation(55, 77) == 9
    duty = (C.c_uint32 * 8)()
    DLL.FHD_GetDuty.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    assert not DLL.FHD_GetDuty(55, duty)
    assert not DLL.FHD_GetDuty(77, duty)
    assert DLL.FHD_GetDuty(88, duty) and (duty[0], duty[1]) == (1, 9)
    assert snapshot().startswith(b'FHDNPC08')


def test_market_reports_age_independently_and_survive_reloading():
    DLL.FHD_BridgeNewGame()
    prices = (C.c_uint16 * 8)(100)
    needs = (C.c_uint16 * 8)(120)
    DLL.FHD_WorldObserve(12, 3, 100, prices, needs)
    DLL.FHD_WorldObserve(524, 4, 105, prices, needs)
    data = snapshot()
    DLL.FHD_BridgeNewGame()
    assert restore(data) == 0
    quote, need = C.c_uint32(), C.c_uint32()
    def report(planet, star, day):
        return DLL.FHD_WorldQuote(planet, star, day, 0, C.byref(quote), C.byref(need))
    assert report(12, 3, 110) and (quote.value, need.value) == (90, 96)
    assert report(524, 4, 110) and (quote.value, need.value) == (95, 108)
    assert not report(12, 4, 110)
    assert not report(12, 3, 151)


def test_exact_takeoff_call_used_for_a_waiting_trader():
    exe = os.environ.get('FHD_TARGET_EXE')
    if not exe:
        pytest.skip('set FHD_TARGET_EXE')
    data = Path(exe).read_bytes()
    at = 0x1ac407
    call = data[at - 0xc00:at - 0xc00 + 5]
    assert call == bytes.fromhex('e8 5c 65 13 00')
    assert at + 5 + struct.unpack('<i', call[1:])[0] == 0x2e2968
    assert data[0x2e2968 - 0xc00:0x2e296d - 0xc00] == bytes.fromhex('53 56 57 55 83')
