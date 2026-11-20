#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
clang -shared -fPIC -O2 -std=c11 -D_POSIX_C_SOURCE=200809L \
 -o build/libfhd_npc_bridge_test.so src/fhd_dialogue.c src/fhd_dialogue_settings.c src/fhd_world.c src/fhd_npc_ai.c src/fhd_npc_bridge.c \
 src/fhd_npc_sidecar.c src/fhd_live_policy.c src/fhd_profession_policy.c src/fhd_grievance_rules.c tests/fixtures/social_bridge.c
clang -shared -fPIC -O2 -std=c11 -o build/libfhd_social_rules_test.so src/fhd_social_rules.c src/fhd_profession_policy.c src/fhd_grievance_rules.c
python -m pytest -q tests "$@"
