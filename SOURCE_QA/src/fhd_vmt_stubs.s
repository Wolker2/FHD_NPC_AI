.text
/* Each stub keeps a per-call 48-byte frame (32 bytes currently used).
   EAX is Delphi Self; preserve EDX/ECX and all flags before vanilla call.
   A post-call observer restores temporary preferences and records results.
   If the original throws a Delphi exception, native stack unwinding bypasses
   the post-hook; keep this build TEST ONLY and never hot-unload. */
.macro STUB number
.globl _FHDNPC_Stub\number
_FHDNPC_Stub\number:
    pushl %eax
    subl $48, %esp
    pushfl
    pushal
    movl 84(%esp), %eax
    leal 36(%esp), %edx
    pushl %edx
    pushl $\number
    pushl %eax
    call _FHDNPC_BeforeShipTick
    addl $12, %esp
    popal
    popfl
    movl 48(%esp), %eax
    call *_fhd_orig\number
    pushfl
    pushal
    movl 84(%esp), %eax
    leal 36(%esp), %edx
    pushl %edx
    pushl $\number
    pushl %eax
    call _FHDNPC_AfterShipTick
    addl $12, %esp
    popal
    popfl
    leal 52(%esp), %esp
    ret
.endm
STUB 0
STUB 1
STUB 2
STUB 3
STUB 4
STUB 5
