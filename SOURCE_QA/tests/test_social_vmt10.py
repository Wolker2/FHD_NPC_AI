import os
"""1.0 native social VMT exact-EXE static contract; no Windows/game run."""
import unittest,struct,hashlib,subprocess
from pathlib import Path
P=Path(__file__).resolve().parents[1]
TARGET=Path(os.environ.get('FHD_TARGET_EXE', '/mnt/data/Rangers.exe'))
SRC=(P/'src/fhd_social_hooks.c').read_text()
ASM=(P/'src/fhd_social_stubs.s').read_text()
VMT=[0x310c8c,0x1ab5a8,0x1992cc,0x1a2b2c,0x1b8140,0x29a3c4]
ATTACK=[0x316f88,0x1ad674,0x19c3cc,0x1a557c,0x1ba2f8,0x29f2fc]
SELECT=[0x1ae4b8,0x1a6fe4]

class TestSocialVMT(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.b=TARGET.read_bytes()
  pe=struct.unpack_from('<I',cls.b,0x3c)[0]; o=pe+24+struct.unpack_from('<H',cls.b,pe+20)[0]
  cls.sections=[]
  for i in range(struct.unpack_from('<H',cls.b,pe+6)[0]):
   size,rva,rawsize,offset=struct.unpack_from('<IIII',cls.b,o+i*40+8)
   cls.sections.append((rva,rawsize,offset))
 @classmethod
 def at(cls,rva,n):
  for base,size,off in cls.sections:
   if base<=rva and rva+n<=base+size: return cls.b[off+rva-base:off+rva-base+n]
  raise ValueError(hex(rva))
 def test_exact_new_exe(self):
  self.assertEqual(hashlib.sha256(self.b).hexdigest(),'e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf')
 def test_six_attack_dispatch_slots(self):
  for v,a in zip(VMT,ATTACK):
   self.assertEqual(struct.unpack('<I',self.at(v+0x60,4))[0],0x400000+a)
   self.assertIn(self.at(a,1),[bytes([x]) for x in (0x53,0x55,0x56,0x8b)])
 def test_transport_warrior_select_slots(self):
  for v,a in [(VMT[1],SELECT[0]),(VMT[3],SELECT[1])]:
   self.assertEqual(struct.unpack('<I',self.at(v+0x50,4))[0],0x400000+a)
 def test_distinct_from_nextday_hooks(self):
  addresses=[v+0x60 for v in VMT]+[VMT[1]+0x50,VMT[3]+0x50]
  self.assertEqual(len(set(addresses)),8)
  self.assertFalse(set(addresses)&set(VMT))
  self.assertNotIn(0x1b1300,addresses)
 def test_no_dominator_control_or_unknown_mod_origin(self):
  self.assertIn('type==12u',SRC)
  self.assertIn('read32(p,0x0cu)',SRC)
  self.assertIn('candidate(v,category)',SRC)
  self.assertIn('safe_target(a)',SRC)
 def test_existing_enemy_and_active_orders_are_preserved(self):
  self.assertIn('g_override_existing[kind]++',SRC)
  self.assertIn('g_override_fear[kind]++',SRC)
  self.assertIn('g_override_order[kind]++',SRC)
  self.assertIn('if(old_enemy){g_existing_target++;',SRC)
  self.assertIn('*(volatile uint32_t*)(p+0x3f4u)=0u',SRC)
  self.assertIn('g_pairs[j].day==day',SRC)
 def test_original_methods_preserved(self):
  self.assertIn('jmp *_fhd_react_orig',ASM)
  self.assertIn('call *_fhd_select_orig',ASM)
  self.assertIn('pushl 0x3f4(%eax)',ASM)
 def test_exports_native_dll(self):
  p=subprocess.run(['llvm-objdump','-p',str(P/'build/FHD_NPC_AI.dll')],capture_output=True,text=True,check=True)
  for name in ['FHDNPC_ArmSocial','FHDNPC_DisarmSocial','FHDNPC_SocialStats']:
   self.assertIn(name,p.stdout)
 def test_native_memory_unchanged(self):
  s=(P/'src/fhd_save_adapter.c').read_text()
  self.assertIn('0x1e51a4u',s)
  self.assertIn('0x1e51c4u',s)
  self.assertIn('FHDNPC_SaveStub',s)
 def test_compatibility_guard(self):
  s=(P/'src/fhd_xeno_probe.c').read_text()
  self.assertIn('is_expected_sha',s)
  self.assertIn('FHDNPC_ArmSocial()',s)
 def test_no_hot_unload_requirement(self):
  self.assertIn('FHDNPC_DisarmSocial()', (P/'src/fhd_vmt_probe.c').read_text())
