"""Line selection, independent relationship effects, and exact Win32 call sites."""
from collections import defaultdict
import ctypes as C
from pathlib import Path

from test_bridge import DLL, ROOT, put, ship, snapshot
from test_foundation18 import restore
from test_native_grievance17_static import at


def read_line():
    buffer = (C.c_uint16 * 650)()
    size = DLL.FHDNPC_DialoguePopW(buffer, len(buffer))
    return bytes(buffer)[:size * 2].decode("utf-16-le") if size > 0 else ""


def test_every_event_has_an_authored_catalog_for_all_five_races():
    events = ("loot_taken", "loot_destroyed", "threat", "demand", "attack", "contract",
              "rivalry", "agreement", "delivery", "warning", "pirate_take", "pirate_destroy")
    lines = defaultdict(set)
    for row in (ROOT / "src/fhd_dialogue_ru.tsv").read_text().splitlines():
        if row.startswith("#"):
            continue
        event, race, caption = row.split("|")
        lines[event, int(race)].add(caption)
    assert len(lines) == len(events) * 5
    assert DLL.FHDNPC_DialogueVariants() == len(set.union(*lines.values())) == 215
    DLL.FHDNPC_DialogueModeSet(2)
    DLL.FHDNPC_DialogueReset()
    for event, key in enumerate(events, 1):
        DLL.FHD_DialogueContext(5, event)
        for race in range(5):
            assert queue(event, race, 44, 11, 1, event, race) == 1
            assert read_line() in lines[key, race]


DLL.FHD_DialogueNamed.argtypes = [C.c_uint32] * 8 + [C.POINTER(C.c_uint16)]
DLL.FHD_DialogueContext.argtypes = [C.c_uint32, C.c_uint32]
DLL.FHD_DialogueContext.restype = None


def queue(event, race, actor, target, personal, day, salt, system=5, name=None):
    return DLL.FHD_DialogueNamed(event, race, actor, target, personal, day, salt, system, name)


def test_personal_mode_limits_deduplicates_and_copies_the_name():
    DLL.FHDNPC_DialogueReset()
    DLL.FHD_DialogueContext(5, 100)
    DLL.FHDNPC_DialogueModeSet(0)
    assert not queue(1, 0, 44, 11, 1, 100, 1)
    DLL.FHDNPC_DialogueModeSet(1)
    assert not queue(11, 4, 44, 11, 0, 100, 1)
    name = (C.c_uint16 * 9)(*map(ord, "А<строн>"))
    assert queue(11, 4, 44, 11, 1, 100, 1, name=name) == 1
    name[0] = ord("Я")
    assert queue(11, 4, 44, 11, 1, 100, 1) == 0
    assert DLL.FHDNPC_DialoguePopW(None, 0) < 0
    assert read_line().startswith("<color=187,154,38>А строн :</color> ")
    assert queue(12, 4, 44, 11, 1, 100, 2) == 1
    assert not queue(12, 4, 44, 11, 1, 100, 3)
    DLL.FHD_DialogueContext(5, 101)
    assert queue(12, 4, 44, 11, 1, 101, 3) == 1
    assert read_line() and not read_line()
    DLL.FHDNPC_DialogueModeSet(0)


def test_settings_abi_matches_exact_rangers_exe():
    assert at(0x13720b, 10) == bytes.fromhex("c7 83 d4 00 00 00 03 00 00 00")
    assert at(0x13ffd0, 8) == bytes.fromhex("33 c0 89 83 d0 00 00 00")
    assert at(0x13a8f2, 3) == bytes.fromhex("c2 04 00")
    assert at(0x13ad05, 3) == bytes.fromhex("c2 08 00")
    assert at(0x13a9f0, 3) == bytes.fromhex("2b 45 f0")
    assert at(0x13ac04, 3) == bytes.fromhex("8b 83 d8")
    assert at(0x3a5dc0, 11) == bytes.fromhex("55 8b ec 83 c4 f4 53 56 57 8b f9")


def test_incoming_lots_expire_and_rebuild_after_load():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=44)) and put(ship(id=45))
    first = (C.c_uint32 * 5)(123, 140, 2, 37, 17)
    second = (C.c_uint32 * 5)(123, 120, 2, 53, 13)
    DLL.FHD_SetTradeGoal.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    DLL.FHD_SetTradeGoal(44, first)
    DLL.FHD_SetTradeGoal(45, second)
    DLL.FHD_SetTradeOrigin(44, 111)
    incoming = DLL.FHD_IncomingCargo
    assert incoming(123, 2, 0, 120) == 90
    assert incoming(123, 2, 44, 120) == 53
    assert incoming(123, 2, 0, 121) == 37
    data = snapshot()
    DLL.FHD_BridgeNewGame()
    assert restore(data) == 2
    assert incoming(123, 2, 0, 120) == 90 and DLL.FHD_TradeOrigin(44) == 111
    first[0] = 124
    DLL.FHD_SetTradeGoal(44, first)
    assert incoming(123, 2, 0, 120) == 53 and incoming(124, 2, 0, 120) == 37
    assert DLL.FHD_Forget(45)
    DLL.FHD_SetTradeGoal(44, None)
    assert not incoming(123, 2, 0, 120) and not incoming(124, 2, 0, 120)
    assert not DLL.FHD_TradeOrigin(44)


def test_interracial_supply_is_settled_only_for_matching_deliveries():
    DLL.FHD_BridgeNewGame()
    assert DLL.FHD_WorldAgree(12, 13, 1, 85, 100)
    assert not DLL.FHD_WorldAgree(12, 13, 1, 85, 101)
    assert not DLL.FHD_WorldFulfil(99, 13, 1, 99, 101)
    assert DLL.FHD_WorldFulfil(12, 13, 1, 40, 101) == 1
    data = snapshot()
    DLL.FHD_BridgeNewGame()
    assert restore(data) == 0
    assert DLL.FHD_WorldAgreement(12, 13, 1, 102) == 45
    assert DLL.FHD_WorldFulfil(12, 13, 1, 49, 102) == 2
    assert not DLL.FHD_WorldFulfil(12, 13, 1, 49, 103)
    assert DLL.FHD_WorldAgree(12, 13, 1, 85, 136)


def test_migrates_actual_2_0_market_and_trade_records():
    data = (Path(__file__).parent / "fixtures" / "fhd_npc_ai_2_0_world.bin").read_bytes()
    assert data.startswith(b"FHDNPC06")
    assert restore(data) == 1
    assert DLL.FHD_IncomingCargo(123, 2, 0, 120) == 37
    assert not DLL.FHD_TradeOrigin(44)
    quote, need = C.c_uint32(), C.c_uint32()
    assert DLL.FHD_WorldQuote(123, 15, 120, 2, C.byref(quote), C.byref(need))
    assert (quote.value, need.value) == (90, 80)


def test_space_settings_native_fourth_button_and_planetary_modes():
    # The game already dispatches ButGroup3 with the proper native panel and selected state.
    assert at(0x13a688, 20) == "ButGroup3\0".encode("utf-16-le")
    assert at(0x13a483, 6) == bytes.fromhex("83 fb 03 0f 94 c2")
    assert at(0x13a5b8, 7) == bytes.fromhex("83 bb d0 00 00 00 03")
    assert at(0x13f0b8, 2) == bytes.fromhex("75 17")
    assert at(0x13f1ad, 9) == bytes.fromhex("33 d2 8b c6 8b 08 ff 51 28")
    assert at(0x13f69e, 7) == bytes.fromhex("33 d2 8b 08 ff 51 28")
    assert at(0x0acadc, 8) == bytes.fromhex("53 56 57 83 c4 f8 8b fa")



def test_local_scope_excludes_npc_loot_and_discards_unheard_radio_on_jump():
    DLL.FHDNPC_DialogueReset()
    DLL.FHDNPC_DialogueModeSet(2)
    DLL.FHD_DialogueContext(5, 100)
    for loot in (1, 2, 3, 4, 11, 12):
        assert queue(loot, 2, 61, 62, 0, 100, 1) == 0
    assert not queue(6, 0, 61, 62, 0, 100, 1, system=7)
    assert queue(6, 0, 61, 62, 0, 100, 1) == 1
    DLL.FHD_DialogueContext(0, 100)
    assert not read_line()
    assert not queue(6, 0, 61, 62, 0, 100, 2, system=0)
    DLL.FHD_DialogueContext(7, 100)
    assert not queue(6, 0, 61, 62, 0, 100, 3)
    assert queue(6, 0, 61, 62, 0, 100, 4, system=7) == 1
    assert read_line()


def test_no_repetition_across_speakers_systems_and_saved_game():
    DLL.FHD_BridgeNewGame()
    DLL.FHDNPC_DialogueModeSet(2)
    heard = set()
    for day in range(200, 202):
        DLL.FHD_DialogueContext(5, day)
        assert queue(10, 4, day, 55, 0, day, 1) == 1
        heard.add(read_line())
    assert len(heard) == 2
    saved = snapshot()
    assert saved.startswith(b"FHDNPC08")
    assert restore(saved) == 0
    DLL.FHD_DialogueContext(7, 204)
    assert queue(10, 4, 62, 63, 0, 204, 2, system=7) == 1
    assert read_line() not in heard
    assert queue(10, 3, 62, 63, 0, 204, 3, system=7) == 1
    assert read_line() not in heard


def test_version_211_snapshot_imports_without_radio_history():
    raw = (Path(__file__).parent / "fixtures" / "fhd_npc_ai_2_1_world.bin").read_bytes()
    assert raw.startswith(b"FHDNPC07") and restore(raw) == 1
    assert DLL.FHD_IncomingCargo(123, 2, 0, 120) == 37
    DLL.FHDNPC_DialogueModeSet(1)
    DLL.FHD_DialogueContext(5, 121)
    assert queue(1, 1, 41, 20, 1, 121, 1) == 1
    assert read_line()


def test_local_radio_calls_match_supported_executable():
    assert at(0x347300, 11) == bytes.fromhex("a1 fc 00 81 00 35 d3 15 cd b1 c3")
    assert at(0x3a5eee, 3) == bytes.fromhex("8b 45 f4")
    assert at(0x3a5ef7, 3) == bytes.fromhex("c2 04 00")
