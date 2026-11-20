/* Exact Delphi register ABI for AddOrUpdatePlayerBubble Kind: AL, Turn: EDX,
 * const Text: WideString in ECX, const Key: WideString on stack (ret 4).
 * Caller is MSVC cdecl, hence decorated symbol starts with an underscore. */
.text
.globl _FHDNPC_InvokeNews
_FHDNPC_InvokeNews:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 movl 8(%ebp),%ebx
 movl 12(%ebp),%eax
 movl 16(%ebp),%edx
 movl 20(%ebp),%ecx
 pushl $0
 call *%ebx
 popl %edi
 popl %esi
 popl %ebx
 movl %ebp,%esp
 popl %ebp
 ret
