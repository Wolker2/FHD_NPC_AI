.text
.globl _FHD_SettingsBuildStub
_FHD_SettingsBuildStub:
 pushfl
 pushal
 pushl %ebx
 call _FHD_SettingsBuild
 addl $4,%esp
 popal
 popfl
 movl $3,0xd4(%ebx)
 ret
.globl _FHD_SettingsApplyStub
_FHD_SettingsApplyStub:
 xorl %eax,%eax
 movl %eax,0xd0(%ebx)
 pushfl
 pushal
 pushl %ebx
 call _FHD_SettingsApply
 addl $4,%esp
 popal
 popfl
 ret
/* RefreshModeUi uses the same virtual SetActive call for its fourth button.
 * The stock Planetary Battles branch still hides the button. */
.globl _FHD_SettingsShowTabStub
_FHD_SettingsShowTabStub:
 pushl %ebx
 call _FHD_SettingsShowTab
 addl $4,%esp
 movl %eax,%edx
 movl %esi,%eax
 movl (%eax),%ecx
 jmp *0x28(%ecx)
.globl _FHD_SettingsChoice
_FHD_SettingsChoice:
 pushl %ebp
 movl %esp,%ebp
 movl 12(%ebp),%eax
 movl 16(%ebp),%edx
 movl 20(%ebp),%ecx
 pushl 24(%ebp)
 pushl $0
 call *8(%ebp)
 popl %ebp
 ret

.globl _FHD_SettingsLabel
_FHD_SettingsLabel:
 movl 8(%esp),%eax
 movl 12(%esp),%edx
 movl 16(%esp),%ecx
 pushl %ebx
 movl 8(%esp),%ebx
 pushl $0
 call *%ebx
 popl %ebx
 ret
