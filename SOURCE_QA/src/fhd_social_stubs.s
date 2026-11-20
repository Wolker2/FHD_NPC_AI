.text
/* VMT ReactToAttack(Self=EAX, Attacker=EDX). Restore all registers/flags
   and tail-jump to the original method. Never manufacture game orders. */
.macro REACT num
.globl _FHDNPC_ReactStub\num
_FHDNPC_ReactStub\num:
    pushl %eax
    pushl %edx
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 36(%esp), %edx
    pushl $\num
    pushl %edx
    pushl %eax
    call _FHDNPC_ObserveAttack
    addl $12, %esp
    popal
    popfl
    movl 4(%esp), %eax
    movl (%esp), %edx
    addl $8, %esp
    jmp *_fhd_react_orig\num
.endm
REACT 0
REACT 1
REACT 2
REACT 3
REACT 4
REACT 5
/* SelectEnemyShipInStar(Self=EAX) invokes the untouched native selection,
   then can cancel a fresh non-essential acquisition (no active order). */
.macro SELECT num
.globl _FHDNPC_SelectStub\num
_FHDNPC_SelectStub\num:
    pushl %eax
    pushl 0x3f4(%eax)
    movl 4(%esp), %eax
    call *_fhd_select_orig\num
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 36(%esp), %edx
    pushl %edx
    pushl $\num
    pushl %eax
    call _FHDNPC_FilterSelectedEnemy
    addl $12, %esp
    popal
    popfl
    addl $8, %esp
    ret
.endm
SELECT 0
SELECT 1
SELECT 2

/* TPirate.RecomputeFearState: original Delphi Boolean in AL. Preserve Self
   and EFLAGS, return original except when the bounded hop-loop policy opts
   for a tactical stand by an armed, sufficiently strong pirate. */
.globl _FHDNPC_TransportFearStub
_FHDNPC_TransportFearStub:
    pushl %eax
    call *_fhd_transport_fear_orig
    pushl %eax
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 36(%esp), %edx
    pushl %edx
    pushl %eax
    call _FHDNPC_PostTransportFear
    addl $8, %esp
    movl %eax, 36(%esp)
    popal
    popfl
    popl %eax
    addl $4, %esp
    ret
/* TTransport.TrustsAttackRequester(Self=EAX, Requester=EDX) -> AL. */
.globl _FHDNPC_TransportTrustStub
_FHDNPC_TransportTrustStub:
    pushl %eax
    pushl %edx
    call *_fhd_transport_trust_orig
    pushl %eax
    pushfl
    pushal
    movl 44(%esp), %eax
    movl 40(%esp), %edx
    movl 36(%esp), %ecx
    pushl %ecx
    pushl %edx
    pushl %eax
    call _FHDNPC_PostTransportTrust
    addl $12, %esp
    movl %eax, 36(%esp)
    popal
    popfl
    popl %eax
    addl $8, %esp
    ret
/* TTransport.ReactToExtortionDemand(Self=EAX, Requester=EDX). We observe
   only AFTER original code has received the demand. */
.globl _FHDNPC_TransportExtortionStub
_FHDNPC_TransportExtortionStub:
    pushl %eax
    pushl %edx
    call *_fhd_transport_extortion_orig
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 36(%esp), %edx
    pushl %edx
    pushl %eax
    call _FHDNPC_ObserveExtortion
    addl $8, %esp
    popal
    popfl
    addl $8, %esp
    ret
