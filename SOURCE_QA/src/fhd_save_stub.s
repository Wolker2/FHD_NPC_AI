/* Delphi TSaver.Execute uses EAX=Self; zero stack arguments.
   Entry/exit callbacks preserve flags and general-purpose registers. */
.text
.globl _FHDNPC_SaveStub
_FHDNPC_SaveStub:
    pushl %eax
    pushfl
    pushal
    movl 36(%esp), %eax
    pushl %eax
    call _FHDNPC_SaverBefore
    addl $4, %esp
    popal
    popfl
    popl %eax
    pushl %eax
    call *_fhd_save_original
    pushfl
    pushal
    movl 36(%esp), %eax
    pushl %eax
    call _FHDNPC_SaverAfter
    addl $4, %esp
    popal
    popfl
    addl $4, %esp
    ret
