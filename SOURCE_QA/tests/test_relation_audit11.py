"""Read-only relationship fingerprint survives SNAPSHOT/RESTORE, not just count."""
import ctypes as C
from test_bridge import DLL,ship,put,record
DLL.FHD_RelationAudit.argtypes=[C.POINTER(C.c_uint32),C.c_uint32]
DLL.FHD_RelationAudit.restype=C.c_int32
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_Relation.restype=C.c_int32

def audit():
 out=(C.c_uint32*6)()
 assert DLL.FHD_RelationAudit(out,6)==1
 return tuple(out)

def snapshot():
 n=DLL.FHD_Snapshot(None,0);p=(C.c_uint8*n)()
 assert DLL.FHD_Snapshot(p,n)==n
 return p,n

def test_count_is_not_a_relationship_checksum():
 DLL.FHD_BridgeNewGame();assert put(ship(id=101))==1
 before=audit()
 assert record(101,202,2,1,101)==1
 after=audit()
 assert before[0]==after[0]==1
 assert before[5]!=after[5] and after[4]==1

def test_relationships_persist_with_identical_fingerprint():
 DLL.FHD_BridgeNewGame();assert put(ship(id=101))==1
 assert put(ship(id=102))==1
 assert record(101,102,2,1,100)==1
 assert record(102,101,1,1,100)==1
 before=audit();rel=DLL.FHD_Relation(101,102)
 buff,n=snapshot()
 DLL.FHD_BridgeNewGame();assert DLL.FHD_Restore(buff,n)==2
 assert audit()==before
 assert DLL.FHD_Relation(101,102)==rel

def test_audit_readonly_and_argument_validation():
 DLL.FHD_BridgeNewGame();put(ship(id=101));record(101,202,2,1,100)
 before,n=snapshot();first=audit()
 assert first==audit()
 assert DLL.FHD_RelationAudit(None,6)==0
 assert DLL.FHD_RelationAudit((C.c_uint32*6)(),5)==0
 after,m=snapshot();assert n==m and bytes(before)==bytes(after)
