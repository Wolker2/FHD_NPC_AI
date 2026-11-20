.text
/* Only the PLAYER'S inherited ReactToAttack VMT entry is observed.
   EAX=player, EDX=attacker. Original method always executes. */
.globl _FHDNPC_PlayerReactStub
_FHDNPC_PlayerReactStub:
 pushl %eax
 pushl %edx
 pushfl
 pushal
 movl 40(%esp), %eax
 movl 36(%esp), %edx
 pushl %edx
 pushl %eax
 call _FHDNPC_ObservePlayerAttacked
 addl $8,%esp
 popal
 popfl
 movl 4(%esp),%eax
 movl 0(%esp),%edx
 addl $8,%esp
 jmp *_fhd_player_react_orig
/* TRanger.ApplyAttackReputationChanges. EAX=self, EDX=victim,
   [ESP+4..+11]=Delphi Double severity, original RET 8 cleans the stack.
   Preserve achievements: invoke the COMPLETE original function and alter
   only severity when a verified first-strike record proves self-defense. */
.globl _FHDNPC_ReputationStub
_FHDNPC_ReputationStub:
 pushfl
 pushal
 movl 28(%esp),%eax
 movl 20(%esp),%edx
 pushl %edx
 pushl %eax
 call _FHDNPC_ShouldSuppressReputation
 addl $8,%esp
 testl %eax,%eax
 jz 1f
 movl $0,40(%esp)
 movl $0,44(%esp)
1:
 popal
 popfl
 pushl %ebp
 movl %esp,%ebp
 subl $0x24,%esp
 jmp *_fhd_reputation_continue
/* The native function's final HomePlanet update is independent of
   Severity! This second guard skips only that end branch after full native
   achievements/relations were processed with Severity=0. Original EBP is
   preserved and EDI still holds victim; original epilogue is at HOME_END. */
.globl _FHDNPC_ReputationHomeStub
_FHDNPC_ReputationHomeStub:
 pushfl
 pushal
 movl -4(%ebp),%eax
 pushl %edi
 pushl %eax
 call _FHDNPC_IsDefenseVerified
 addl $8,%esp
 testl %eax,%eax
 jnz 1f
 popal
 popfl
 movl 0x2c(%edi),%ebx
 testl %ebx,%ebx
 jmp *_fhd_home_continue
1:
 popal
 popfl
 jmp *_fhd_home_end
/* TStar.CompleteItemPickup nested cdecl(ParentFrame); capture interested
   NPCs BEFORE ClearItemReferences and commit ONLY after actual removal. */
.globl _FHDNPC_PickupStub
_FHDNPC_PickupStub:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 subl $192,%esp
 leal -160(%ebp),%eax
 pushl %eax
 pushl 8(%ebp)
 call _FHDNPC_CaptureLootPickup
 addl $8,%esp
 pushl 8(%ebp)
 call _fhd_pickup_trampoline
 addl $4,%esp
 leal -160(%ebp),%eax
 pushl %eax
 call _FHDNPC_CompleteLoot
 addl $4,%esp
 addl $192,%esp
 popl %edi
 popl %esi
 popl %ebx
 popl %ebp
 ret
_fhd_pickup_trampoline:
 pushl %ebp
 movl %esp,%ebp
 pushl %ecx
 pushl %ebx
 jmp *_fhd_pickup_continue
/* TShip.FireWeaponAtItem EAX=ship EDX=weapon ECX=item;
   [ESP+4] RecordFilm. Original decides whether item actually exploded. */
.globl _FHDNPC_ShotStub
_FHDNPC_ShotStub:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 subl $208,%esp
 movl %eax,-16(%ebp)
 movl %edx,-20(%ebp)
 movl %ecx,-24(%ebp)
 leal -192(%ebp),%eax
 pushl %eax
 pushl -24(%ebp)
 pushl -16(%ebp)
 call _FHDNPC_CaptureLootShot
 addl $12,%esp
 pushl 8(%ebp)
 movl -16(%ebp),%eax
 movl -20(%ebp),%edx
 movl -24(%ebp),%ecx
 call _fhd_shot_trampoline
 /* The vanilla FireWeaponAtItem returns with RET 4 and already pops its
    Boolean stack argument. Adding another four here corrupts the frame. */
 leal -192(%ebp),%eax
 pushl %eax
 call _FHDNPC_CompleteLoot
 addl $4,%esp
 addl $208,%esp
 popl %edi
 popl %esi
 popl %ebx
 popl %ebp
 ret $4
_fhd_shot_trampoline:
 pushl %ebp
 movl %esp,%ebp
 subl $0x3c,%esp
 jmp *_fhd_shot_continue
/* TNormalShip.ProcessShipKill(EAX=killer, EDX=victim) is called before
   final object deletion. After native rewards, pay a *verified* bounty. */
.globl _FHDNPC_KillStub
_FHDNPC_KillStub:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 subl $16,%esp
 movl %eax,-16(%ebp)
 movl %edx,-20(%ebp)
 movl -16(%ebp),%eax
 movl -20(%ebp),%edx
 call _fhd_kill_trampoline
 pushl -20(%ebp)
 pushl -16(%ebp)
 call _FHDNPC_OnNativeKill
 addl $8,%esp
 addl $16,%esp
 popl %edi
 popl %esi
 popl %ebx
 popl %ebp
 ret
_fhd_kill_trampoline:
 pushl %ebp
 movl %esp,%ebp
 movl $14,%ecx
 jmp *_fhd_kill_continue
/* Verified vanilla TShip/TPlanet methods; no direct writes to encoded fields. */
.globl _FHDNPC_InvokeJoint
_FHDNPC_InvokeJoint:
 movl 4(%esp),%eax
 movl 8(%esp),%edx
 movl 12(%esp),%ecx
 jmp *_fhd_joint_attack
.globl _FHDNPC_InvokeSetMoney
_FHDNPC_InvokeSetMoney:
 movl 4(%esp),%eax
 movl 8(%esp),%edx
 jmp *_fhd_set_money
.globl _FHDNPC_InvokePlanetRelation
_FHDNPC_InvokePlanetRelation:
 movl 4(%esp),%eax
 movl 8(%esp),%edx
 movl 12(%esp),%ecx
 jmp *_fhd_planet_relation
/* Existing ClearItemReferences calls on the two verified missile paths.
   TStar is in EAX, TItem in EDX, the impacting TMissile in EBX. */
.globl _FHDNPC_MissileLootStub
_FHDNPC_MissileLootStub:
 pushfl
 pushal
 pushl %ebx
 pushl %edx
 pushl %eax
 call _FHDNPC_MissileLoot
 addl $12,%esp
 popal
 popfl
 jmp *_fhd_clear_item
/* Raw unhooked prologue trampoline for TShip.GetRelationToShip (0x2F3664). */
.globl _FHDNPC_RelationRawTrampoline
_FHDNPC_RelationRawTrampoline:
 pushl %ebx
 pushl %esi
 pushl %edi
 pushl %ebp
 movl %edx,%edi
 jmp *_fhd_relation_continue
/* Direct prologue detour on TShip.GetRelationToShip (0x2F3664) so all 55
   callers in Rangers.exe (UI cards, SelectEnemy, GetRelationLevelToShip)
   receive the memory-adjusted relation. */
.globl _FHDNPC_RelationValueStub
_FHDNPC_RelationValueStub:
 pushl %ebp
 movl %esp,%ebp
 pushl %eax
 pushl %edx
 call _FHDNPC_RelationRawTrampoline
 movzbl %al,%eax
 pushl %eax
 pushl -8(%ebp)
 pushl -4(%ebp)
 call _FHDNPC_RelationLevelValue
 movl %ebp,%esp
 popl %ebp
 ret
/* Called from GetRelationLevelToShip, after the native calculation. */
.globl _FHDNPC_RelationLevelStub
_FHDNPC_RelationLevelStub:
 pushl %ebp
 movl %esp,%ebp
 pushl %eax
 pushl %edx
 call *_fhd_relation_value
 movzbl %al,%eax
 pushl %eax
 pushl -8(%ebp)
 pushl -4(%ebp)
 call _FHDNPC_RelationLevelValue
 movl %ebp,%esp
 popl %ebp
 ret

/* TShip.TryExtortShip (0x006D4390): EAX=requester, EDX=player.
   This is the engine's own money/goods dialogue, not a fake radio prompt. */
.globl _FHDNPC_InvokeExtort
_FHDNPC_InvokeExtort:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 movl 8(%ebp),%ecx
 movl 12(%ebp),%eax
 movl 16(%ebp),%edx
 call *%ecx
 movzbl %al,%eax
 popl %edi
 popl %esi
 popl %ebx
 popl %ebp
 ret

/* Target-player TRanger.BuildMoneyExtortionResponse, RVA 0x31A298:
   Delphi register ABI: EAX=target player, EDX=issuer, ECX=&WideString,
   4th arg DemandedAmount is one callee-popped stack dword.
   Do not confuse its Boolean return with whether the player saw the modal. */
.globl _FHDNPC_InvokeMoneyDemand
_FHDNPC_InvokeMoneyDemand:
 pushl %ebp
 movl %esp,%ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 movl 8(%ebp),%esi
 movl 12(%ebp),%eax
 movl 16(%ebp),%edx
 movl 20(%ebp),%ecx
 pushl 24(%ebp)
 call *%esi
 movzbl %al,%eax
 popl %edi
 popl %esi
 popl %ebx
 popl %ebp
 ret

/* Experimental pilot personality info in TFormInfo (RVA 0x101a6a).
   Original 9 bytes: mov -0xc(%ebp),%eax ; mov 0x5b1298,%edx
   EBX=index, ESI=FormInfo list owner, [-0xc(%ebp)]=TShip*. */
.globl _FHDNPC_FormInfoStub
_FHDNPC_FormInfoStub:
 pushfl
 pushal
 movl %ebx,%eax
 addl %eax,%eax
 leal (%eax,%eax,4),%eax
 movl 0x10(%esi),%edx
 leal 8(%edx,%eax,8),%eax
 pushl %eax
 pushl -0xc(%ebp)
 call _FHDNPC_AppendShipTraitInfo
 addl $8,%esp
 popal
 popfl
 movl -0xc(%ebp),%eax
 movl 0x5b1298,%edx
 jmp *_fhd_forminfo_continue
