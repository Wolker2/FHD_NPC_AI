"""1.1 deterministic tactical scoring: Linux fixtures, NOT a game run."""
import ctypes as C
from pathlib import Path
DLL=C.CDLL(str(Path(__file__).resolve().parents[1]/'build/libfhd_social_rules_test.so'))
class Profile(C.Structure):
 _pack_=1
 _fields_=[('id',C.c_uint32),('character',C.c_uint8),('role',C.c_uint8),
   ('traits',C.c_uint8*10),('profession',C.c_uint8*3),('reserved',C.c_uint8),('events',C.c_uint32)]
DLL.FHDNPC_AssessEnemy.argtypes=[C.POINTER(Profile),C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32,C.POINTER(C.c_int32)]
DLL.FHDNPC_AssessEnemy.restype=C.c_int32

def assess(kind=0, caution=75, brave=30, aggr=25, greed=75, risk=80,rel=0,dom=0):
 p=Profile();p.traits[0]=brave;p.traits[1]=aggr;p.traits[2]=greed;p.traits[3]=caution
 score=C.c_int32()
 result=DLL.FHDNPC_AssessEnemy(C.byref(p),rel,kind,dom,risk,C.byref(score))
 return result,score.value

def test_cautious_trader_avoids_great_risk():assert assess()[0]==2

def test_same_personality_warrior_more_willing_to_engage():assert assess(kind=1)[0]==0

def test_risk_changes_choice_without_extreme_caution():
 assert assess(kind=0,caution=70,brave=40,aggr=45,greed=30,risk=100)[0]==2
 assert assess(kind=0,caution=70,brave=40,aggr=45,greed=30,risk=10)[0]==0

def test_hostility_increases_willingness_to_fight():
 assert assess(caution=70,brave=35,aggr=20,greed=40,risk=70,rel=0)[0]==2
 assert assess(caution=70,brave=35,aggr=20,greed=40,risk=70,rel=-90)[0]==0

def test_friendship_only_non_dominator():
 assert assess(kind=1,rel=65)[0]==1
 assert assess(kind=1,rel=65,dom=1)[0]!=1

def test_warrior_can_refuse_overwhelming_risk():
 assert assess(kind=1,caution=85,brave=20,aggr=20,greed=0,risk=90)[0]==2

def test_bravery_prevents_avoidance():assert assess(caution=90,brave=85,aggr=40,risk=90)[0]==0

def test_invalid_host_input_fails_open():
 assert assess(kind=2)[0]==0
 assert assess(risk=101)[0]==0

def test_monotone_risk_score():
 vals=[assess(risk=r)[1] for r in [0,20,50,80,100]]
 assert vals==sorted(vals)
