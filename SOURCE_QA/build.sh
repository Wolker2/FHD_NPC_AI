#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
SRC=src
mkdir -p build
for file in fhd_npc_ai fhd_npc_bridge fhd_npc_sidecar fhd_xeno_probe fhd_vmt_probe fhd_live_policy fhd_save_adapter fhd_social_hooks fhd_social_rules fhd_profession_policy fhd_grievance fhd_grievance_rules fhd_station_alert fhd_native_trade fhd_native_duties fhd_world fhd_dialogue fhd_dialogue_settings fhd_news fhd_dialogue_ui; do
 clang -target i686-pc-windows-msvc -std=c11 -O2 -ffreestanding -fno-builtin -Wall -Wextra -Werror -c "$SRC/$file.c" -o "build/$file.obj"
done
clang -target i686-pc-windows-msvc -c "$SRC/fhd_vmt_stubs.s" -o build/fhd_vmt_stubs.obj
clang -target i686-pc-windows-msvc -c "$SRC/fhd_save_stub.s" -o build/fhd_save_stub.obj
clang -target i686-pc-windows-msvc -c "$SRC/fhd_social_stubs.s" -o build/fhd_social_stubs.obj
clang -target i686-pc-windows-msvc -c "$SRC/fhd_grievance_stubs.s" -o build/fhd_grievance_stubs.obj
clang -target i686-pc-windows-msvc -c "$SRC/fhd_news_stub.s" -o build/fhd_news_stub.obj
clang -target i686-pc-windows-msvc -c "$SRC/fhd_dialogue_ui_stub.s" -o build/fhd_dialogue_ui_stub.obj
cat > build/kernel32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
CreateFileW
CloseHandle
ReadFile
WriteFile
GetFileSizeEx
MoveFileExW
DeleteFileW
FlushFileBuffers
GetModuleFileNameW
GetTempPathW
GetModuleHandleW
VirtualProtect
GetCurrentProcess
FlushInstructionCache
MultiByteToWideChar
Sleep
DEF
cat > build/advapi32.def <<'DEF'
LIBRARY ADVAPI32.dll
EXPORTS
CryptAcquireContextW
CryptReleaseContext
CryptCreateHash
CryptHashData
CryptGetHashParam
CryptDestroyHash
DEF
lld-link /lib /def:build/kernel32.def /machine:x86 /out:build/kernel32.lib
lld-link /lib /def:build/advapi32.def /machine:x86 /out:build/advapi32.lib
cat > build/oleaut32.def <<'DEF'
LIBRARY OLEAUT32.dll
EXPORTS
SysAllocStringLen
SysFreeString
DEF
lld-link /lib /def:build/oleaut32.def /machine:x86 /out:build/oleaut32.lib
LLD=(lld-link /DLL /NOLOGO /MACHINE:X86 /SAFESEH:NO /TIMESTAMP:0 /OUT:build/FHD_NPC_AI.dll /ENTRY:DllMain@12 /NODEFAULTLIB)
LLD+=(build/fhd_npc_ai.obj build/fhd_npc_bridge.obj build/fhd_npc_sidecar.obj build/fhd_xeno_probe.obj build/fhd_vmt_probe.obj build/fhd_live_policy.obj build/fhd_save_adapter.obj build/fhd_vmt_stubs.obj build/fhd_save_stub.obj build/fhd_social_hooks.obj build/fhd_social_rules.obj build/fhd_profession_policy.obj build/fhd_social_stubs.obj build/fhd_grievance.obj build/fhd_grievance_rules.obj build/fhd_station_alert.obj build/fhd_grievance_stubs.obj build/fhd_native_trade.obj build/fhd_native_duties.obj build/fhd_world.obj build/fhd_dialogue.obj build/fhd_dialogue_settings.obj build/fhd_news.obj build/fhd_news_stub.obj build/fhd_dialogue_ui.obj build/fhd_dialogue_ui_stub.obj build/oleaut32.lib build/advapi32.lib build/kernel32.lib)
for entry in 'CreateFileW@28' 'CloseHandle@4' 'ReadFile@20' 'WriteFile@20' 'GetFileSizeEx@8' 'MoveFileExW@12' 'DeleteFileW@4' 'FlushFileBuffers@4' 'GetModuleFileNameW@12' 'GetTempPathW@8' 'GetModuleHandleW@4' 'VirtualProtect@16' 'GetCurrentProcess@0' 'FlushInstructionCache@12' 'MultiByteToWideChar@24' 'Sleep@4' 'SysAllocStringLen@8' 'SysFreeString@4' 'CryptAcquireContextW@20' 'CryptReleaseContext@8' 'CryptCreateHash@20' 'CryptHashData@16' 'CryptGetHashParam@20' 'CryptDestroyHash@4'; do
 LLD+=("/alternatename:__imp__$entry=__imp__${entry%%@*}")
done
"${LLD[@]}"
ls -lh build/FHD_NPC_AI.dll
