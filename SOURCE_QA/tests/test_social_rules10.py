"""Pure native choice policy unit tests. No game-process or VMT simulation."""
import ctypes as C
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DLL=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
class Profile(C.Structure):
 _pack_=1
 _fields_=[('id',C.c_uint32),('character',C.c_uint8),('role',C.c_uint8),
          ('traits',C.c_uint8*10),('profession',C.c_uint8*3),('reserved',C.c_uint8),('events',C.c_uint32)]
DLL.FHDNPC_ShouldAvoidEnemy.argtypes=[C.POINTER(Profile),C.c_int32,C.c_uint32,C.c_uint32]
DLL.FHDNPC_ShouldAvoidEnemy.restype=C.c_int32

def prefers(kind=0,brave=20,caution=94,friend=0,dominator=0):
 p=Profile();p.traits[0]=brave;p.traits[3]=caution
 return DLL.FHDNPC_ShouldAvoidEnemy(C.byref(p),friend,kind,dominator)

class TestSocialRules(unittest.TestCase):
 def test_cautious_trader_avoids_target(self):self.assertEqual(prefers(kind=0),1)
 def test_cautious_regular_warrior_avoids(self):self.assertEqual(prefers(kind=1),1)
 def test_brave_trader_does_not_avoid(self):self.assertEqual(prefers(kind=0,brave=80),0)
 def test_brave_warrior_does_not_avoid(self):self.assertEqual(prefers(kind=1,brave=80),0)
 def test_friendship_deters_unnecessary_attack(self):self.assertEqual(prefers(kind=1,brave=80,caution=20,friend=75),1)
 def test_hostility_not_automatically_vetoed(self):self.assertEqual(prefers(kind=1,caution=80,friend=-80),0)
 def test_stations_and_other_roles_unsupported(self):self.assertEqual(prefers(kind=5),0)
 def test_dominator_control_not_issued(self):
  # Deciding not to engage a dominator does not issue commands to its AI.
  self.assertEqual(prefers(kind=0,dominator=1),1)
 def test_cautious_military_threshold_stricter(self):
  self.assertEqual(prefers(kind=0,brave=24,caution=70),1)
  self.assertEqual(prefers(kind=1,brave=24,caution=70),0)
