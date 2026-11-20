"""Read an actual 1.7/ABI 3 snapshot and round-trip its upgraded state."""
from pathlib import Path
from test_bridge import DLL, snapshot
from test_foundation18 import restore


def test_legacy_snapshot_migrates_to_current_layout():
    DLL.FHD_BridgeNewGame()
    old = (Path(__file__).parent / 'fixtures' / 'fhd_npc_ai_1_7_contacts.bin').read_bytes()
    assert restore(old) == 2
    assert DLL.FHD_Relation(55, 77) == -15
    assert DLL.FHD_Relation(55, 90) == 9
    new = snapshot()
    assert old[:8] == b'FHDNPC03' and new[:8] == b'FHDNPC08'
    assert restore(new) == 2
    assert snapshot() == new
