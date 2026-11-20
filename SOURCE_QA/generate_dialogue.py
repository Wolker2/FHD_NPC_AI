"""Compile the author-edited Russian line catalog into the DLL."""
from collections import defaultdict
import json
from pathlib import Path

root = Path(__file__).parent
src = root / "src"
events = ("loot_taken", "loot_destroyed", "threat", "demand", "attack",
          "contract", "rivalry", "agreement", "delivery", "warning",
          "pirate_take", "pirate_destroy")
lines = defaultdict(list)
seen = set()
for row in (src / "fhd_dialogue_ru.tsv").read_text(encoding="utf-8").splitlines():
    if not row or row.startswith("#"):
        continue
    event, race, line = row.split("|")
    race = int(race)
    assert event in events and 0 <= race < 5 and line not in seen
    seen.add(line)
    lines[event, race].append(line)
out = ["/* Generated from fhd_dialogue_ru.tsv. */",
       "static const char *const lines[]={"]
offsets = []
index = 0
for event in events:
    group_offsets = [index]
    for race in range(5):
        variants = lines[event, race]
        assert variants, (event, race)
        out.extend("  " + json.dumps(line, ensure_ascii=False) + "," for line in variants)
        index += len(variants)
        group_offsets.append(index)
    offsets.append(group_offsets)
out.extend(("};", f"#define FHD_AUTHORED_LINES {len(seen)}u",
            "static const uint16_t line_offsets[][6]={"))
for group_offsets in offsets:
    out.append("  {" + ",".join(map(str, group_offsets)) + "},")
out.extend(("};", ""))
(src / "fhd_dialogue_corpus.h").write_text("\n".join(out), encoding="utf-8")
