import hashlib,struct,sys,json
from pathlib import Path
EXE=Path(sys.argv[1])
EXPECTED='e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'
b=EXE.read_bytes()
assert hashlib.sha256(b).hexdigest()==EXPECTED,'EXE hash mismatch -- never hook!'
pe=struct.unpack_from('<I',b,0x3c)[0]
assert b[pe:pe+4]==b'PE\x00\x00'
assert struct.unpack_from('<H',b,pe+4)[0]==0x14c
assert struct.unpack_from('<I',b,pe+8)[0]==0x6AC53B97
assert struct.unpack_from('<I',b,pe+24+28)[0]==0x400000
n=struct.unpack_from('<H',b,pe+6)[0]
opt=pe+24;sec_off=opt+struct.unpack_from('<H',b,pe+20)[0]
sections=[]
for i in range(n):
 j=sec_off+40*i
 name=b[j:j+8].split(b'\x00')[0].decode()
 vsz,rva,fsz,foff=struct.unpack_from('<IIII',b,j+8)
 sections.append((name,rva,vsz,fsz,foff))
def read_va(addr,size):
 rva=addr-0x400000
 for name,base,virtual,raw,off in sections:
  if base<=rva and rva+size<=base+raw:return b[off+(rva-base):off+(rva-base)+size]
 raise RuntimeError('unmapped VA '+hex(addr))
base=0x400000
slots=[0x310c8c,0x1ab5a8,0x1992cc,0x1a2b2c,0x1b8140,0x29a3c4]
orig=[0x31264c,0x1ac298,0x199fb4,0x1a3994,0x1b8c14,0x29c1f8]
rows=[]
for cls,slot,fun in zip(['ranger','transport','pirate','warrior','tranclucator','station'],slots,orig):
 native=struct.unpack('<I',read_va(base+slot,4))[0]
 assert native==base+fun,(cls,hex(native),hex(base+fun))
 assert read_va(base+fun,3)==b'\x55\x8b\xec'
 rows.append({'type':cls,'VMT_slot_RVA':hex(slot),'original_RVA':hex(fun),'verified':True})
# The two native ranger VMTs include a player-derived class; never hook that second table.
assert struct.unpack('<I',read_va(0x747134,4))[0]==0x71264c
# Domiator is explicitly not patched; its VMT is known and audited separately.
assert struct.unpack('<I',read_va(0x5b1300,4))[0]==0x5b32e0
result={'exe_sha256':EXPECTED,'pe_time_date_stamp':'0x6AC53B97','target_slots_checked':rows,
        'player_vmt_not_hooked':True,'dominator_vmt_not_hooked':True}
print(json.dumps(result,indent=2))
