/* Tactical advice and confirmed social events for supported native NPCs. */
#include <stdint.h>
#include "fhd_npc_ai.h"
#include "fhd_world.h"
#include "fhd_social_rules.h"
#include "fhd_profession_policy.h"
#include "fhd_grievance.h"
#if !defined(_WIN32) || !defined(_M_IX86)
#error Supported only for Win32 new Rangers.exe
#endif
#define API __declspec(dllexport)
#define IMP __declspec(dllimport)
#define ST __stdcall
IMP void *ST GetModuleHandleW(const uint16_t *);
IMP int ST VirtualProtect(void *,uint32_t,uint32_t,uint32_t *);
#define EXEC_RW 0x40u
#define MAX_PAIRS 8192u
#define ATTACK_COUNT 6u
#define TRANCLUCATOR_CATEGORY 4u
#define SELECT_COUNT 3u
#define ENTRY_COUNT 12u
#define PLAYER_VMT_NEXT_RVA 0x347134u
/* VMT offsets corroborated with the shipped aShip.pas and actual 2026 EXE. */
static const uint32_t base_vmt_next[6] = {
 0x310c8cu,0x1ab5a8u,0x1992ccu,0x1a2b2cu,0x1b8140u,0x29a3c4u
};
static const uint32_t attack_rva[6] = {
 0x316f88u,0x1ad674u,0x19c3ccu,0x1a557cu,0x1ba2f8u,0x29f2fcu
};
static const uint32_t select_rva[3] = {0x1ae4b8u,0x1a6fe4u,0x19d980u};
/* Verified in the provided PE: transport 0x84 (fear), 0x8c (trust),
   0xa4 (actual extortion reaction). These do not replace dialogue text. */
static uint32_t slot_rva[ENTRY_COUNT], orig_rva[ENTRY_COUNT];
extern void FHDNPC_ReactStub0(void);extern void FHDNPC_ReactStub1(void);
extern void FHDNPC_ReactStub2(void);extern void FHDNPC_ReactStub3(void);
extern void FHDNPC_ReactStub4(void);extern void FHDNPC_ReactStub5(void);
extern void FHDNPC_SelectStub0(void);extern void FHDNPC_SelectStub1(void);
extern void FHDNPC_SelectStub2(void);
extern void FHDNPC_TransportFearStub(void);
extern void FHDNPC_TransportTrustStub(void);
extern void FHDNPC_TransportExtortionStub(void);
void *fhd_react_orig0=0,*fhd_react_orig1=0,*fhd_react_orig2=0;
void *fhd_react_orig3=0,*fhd_react_orig4=0,*fhd_react_orig5=0;
void *fhd_select_orig0=0,*fhd_select_orig1=0,*fhd_select_orig2=0;
void *fhd_transport_fear_orig=0,*fhd_transport_trust_orig=0;
void *fhd_transport_extortion_orig=0;
static void**orig_ptr[ENTRY_COUNT]={
 &fhd_react_orig0,&fhd_react_orig1,&fhd_react_orig2,&fhd_react_orig3,
 &fhd_react_orig4,&fhd_react_orig5,&fhd_select_orig0,&fhd_select_orig1,
 &fhd_select_orig2,&fhd_transport_fear_orig,
 &fhd_transport_trust_orig,&fhd_transport_extortion_orig
};
static void *stub_ptr[ENTRY_COUNT]={
 FHDNPC_ReactStub0,FHDNPC_ReactStub1,FHDNPC_ReactStub2,
 FHDNPC_ReactStub3,FHDNPC_ReactStub4,FHDNPC_ReactStub5,
 FHDNPC_SelectStub0,FHDNPC_SelectStub1,FHDNPC_SelectStub2,
 FHDNPC_TransportFearStub,FHDNPC_TransportTrustStub,
 FHDNPC_TransportExtortionStub
};
static uintptr_t g_base;
static uint32_t g_armed;
static uint32_t g_native_attacks,g_attack_pairs,g_transport_avoided,g_warrior_avoided;
static uint32_t g_filtered_eligible,g_protected,g_target_unavailable,g_duplicate_attacks;
static uint32_t g_select_calls,g_existing_target,g_no_new_target,g_flagship_protected;
static uint32_t g_order_protected,g_self_defense,g_target_protected,g_profile_missing;
static uint32_t g_friend_avoided,g_risk_avoided,g_policy_allowed,g_strength_unknown;
static uint32_t g_betrayals;
static uint32_t g_player_attacks,g_player_rel_targets,g_player_alliances;
static uint32_t g_pirate_calls,g_pirate_economic,g_pirate_revenge;
static uint32_t g_pirate_clan,g_pirate_spare,g_pirate_veto,g_pirate_unverified;
static uint32_t g_pirate_smart_avoid;
static uint32_t g_hop_fear_calls,g_hop_changes,g_hop_interrupted,g_hop_unsafe;
static uint32_t g_transport_fear_checks,g_transport_memory_fear;
static uint32_t g_transport_against_player,g_transport_trust_checks;
static uint32_t g_transport_trust_refused,g_transport_extortion_events;
static uint32_t g_transport_extortion_player,g_transport_memory_unknown;
static uint32_t g_attack_weighted;
/* Diagnostics for order override, incumbent targets and protected targets. */
static uint32_t g_override_order[2],g_override_existing[2],g_override_flagship[2];
static uint32_t g_override_fear[2],g_override_dominator[2],g_no_decision[2];
/* One-time validated contracts and joint fire; never infer rescues. */
static uint32_t g_contracts,g_joint_fights,g_contract_skips;
static uint32_t g_escorts,g_escort_day_skips,g_escorts_ineligible;
static uint32_t g_target_verified,g_target_linked,g_target_rejected;
/* Detailed selection path split by native ship class (transport, warrior). */
static uint32_t g_select_kind[2][6]; /* calls,existing,no_new,orders,protected,eligible */
static struct {uint32_t victim,attacker,vseed,aseed,day;} g_pairs[MAX_PAIRS];
static uint32_t read32(const uint8_t*p,uint32_t off){uint32_t v=0;uint8_t*d=(uint8_t*)&v;uint32_t i;
  for(i=0;i<4u;i++)d[i]=p[off+i];return v;}
/* Exact 2026 game TPlayer VMT. It inherits TRanger but must never be
   upserted or have its own AI controlled by this DLL. It may be a *contact*. */
static int native_player(const uint8_t*p){
 return p && *(const uintptr_t*)p==g_base+PLAYER_VMT_NEXT_RVA-0x1cu &&
        p[0x10u]==1u && p[0x11u]<=4u && !read32(p,0x0cu) &&
        !read32(p,0x3ccu) && read32(p,4u)!=0u && read32(p,4u)!=0xffffffffu;
}
static int candidate(const uint8_t*p,uint32_t category);
/* Verified native strength is cached Single. Guard against NaN/Infinity,
   negative and uninitialized values before comparing threats. */
static uint32_t strength100(const uint8_t*p){
 uint32_t bits;float strength;uint32_t i;uint8_t*dest=(uint8_t*)&strength;
 if(!p)return 0;
 bits=read32(p,0xb8u);
 if(!bits || (bits&0x80000000u) || (bits&0x7f800000u)==0x7f800000u)return 0;
 for(i=0;i<4u;i++)dest[i]=((uint8_t*)&bits)[i];
 if(strength<0.01f || strength>1000000.0f)return 0;
 return (uint32_t)(strength*100.0f);
}
static uint32_t known_transport_profession(const uint8_t*p){
 uint32_t label=FHDNPC_LookupVerifiedProfession(read32(p,4u),
    read32(p,0xe0u),2u);
 return label?label:FHDNPC_Profession(2u,p[0x510u]);
}
/* After the ORIGINAL transport fear calculation: only a known historical
   attacker present in the SAME system may increase fear. The original game
   still chooses destination, negotiates ransom and performs repairs. */
API uint32_t FHDNPC_PostTransportFear(void *ship,uint32_t original){
 uint8_t*p=(uint8_t*)ship,*target;FHDProfile profile;
 uint32_t id,enemy_id,own_power,enemy_power,profession,star;
 int32_t relation;
 if(!g_armed || !p)return original;
 if(original&0xffu){
   if(candidate(p,1u)&&!read32(p,0x1cu)&&!read32(p,0x20u)&&!p[0x459u]&&read32(p,0x100u)!=0u){
     uint8_t *en=(uint8_t*)(uintptr_t)read32(p,0x3f4u);
     if(en && strength100(en)>strength100(p))p[0x3b5u]=1u;
   }
   return original;
 }
 g_transport_fear_checks++;
 if(!candidate(p,1u))return original;
 profession=known_transport_profession(p);
 if(!profession)return original;
 target=(uint8_t*)(uintptr_t)read32(p,0x3f4u);
 if(!target)return original;
 if(!native_player(target) && !(target[0x10u]>=1u &&
   target[0x10u]<=4u && candidate(target,target[0x10u]-1u)))return original;
 star=read32(p,0x24u);
 if(!star || read32(target,0x24u)!=star)return original;
 /* A former attacker who has docked is not presently threatening the ship.
    Honor the game's own active truce before checking old grievances. */
 if(read32(p,0x3f8u)==(uint32_t)(uintptr_t)target ||
    read32(target,0x1cu) || read32(target,0x20u))return original;
 id=read32(p,4u);enemy_id=read32(target,4u);
 if(!id || !enemy_id || id==enemy_id ||
    FHD_ProfileOf(id,&profile)!=1){g_transport_memory_unknown++;return original;}
 relation=FHD_RelationPeer(id,enemy_id,read32(target,0xe0u));
 if(relation>-7)return original;
 own_power=strength100(p);enemy_power=strength100(target);
 if(!own_power || !enemy_power){g_transport_memory_unknown++;return original;}
 /* Hull integrity is not read from unverified pointer offsets; 100% here
    applies only strength/risk criteria. Native fear still handles damage. */
 if(FHDNPC_MemoryFear(profession,&profile,relation,own_power,enemy_power,100u)){
   p[0x3b4u]=1u;g_transport_memory_fear++;
   if(enemy_power>own_power && read32(p,0x100u)!=0u)p[0x3b5u]=1u;
   if(native_player(target))g_transport_against_player++;
   return 1u;
 }
 return original;
}
/* Refuse military requests made by a *confirmed* previous attacker. This is
   the native transport TrustsAttackRequester policy, not a new dialogue. */
API uint32_t FHDNPC_PostTransportTrust(void *ship,void *requester,uint32_t original){
 uint8_t*p=(uint8_t*)ship,*r=(uint8_t*)requester;
 uint32_t profession,id,rid,decision;
 if(!g_armed || !p || !r || !(original&0xffu))return original;
 g_transport_trust_checks++;
 if(!candidate(p,1u) || (!native_player(r) &&
    !(r[0x10u]>=1u && r[0x10u]<=4u && candidate(r,r[0x10u]-1u))))return original;
 id=read32(p,4u);rid=read32(r,4u);
 if(!id || !rid || id==rid)return original;
 profession=known_transport_profession(p);
 decision=FHDNPC_TrustKnownAggressor(1u,FHD_RelationPeer(id,rid,read32(r,0xe0u)),profession);
 if(!decision){g_transport_trust_refused++;return 0u;}
 return original;
}
/* The original ReactToExtortionDemand already adjusted money/reputation as
   needed. We only remember that this requester truly demanded property. */
API void FHDNPC_ObserveExtortion(void *ship,void *requester){
 uint8_t*p=(uint8_t*)ship,*r=(uint8_t*)requester;
 uint32_t id,rid,day;FHDProfile profile;
 if(!g_armed || !p || !r || !candidate(p,1u))return;
 if(!native_player(r) && !candidate(r,0u))return;
 id=read32(p,4u);rid=read32(r,4u);day=read32(p,0xecu);
 if(!id || !rid || id==rid || !day || day>1000000u ||
    FHD_ProfileOf(id,&profile)!=1)return;
 /* One event for the same offender per day, even across SAV reloads. */
 if(FHD_RelationPeerLastDay(id,rid,read32(r,0xe0u))==day)return;
 if(FHD_RecordPeer(id,rid,read32(r,0xe0u),FHD_EVENT_EXTORTION,2u,day)==1){
  g_transport_extortion_events++;
  if(native_player(r))g_transport_extortion_player++;
 }
}

static int candidate(const uint8_t*p,uint32_t category){
 uint32_t type,own;
 if(!p||category>5u || *(uintptr_t*)p!=g_base+base_vmt_next[category]-0x1cu)return 0;
 type=p[0x10u];own=p[0x11u];
 if(!type||type==12u||type>13u||!(own<=4u||own==7u))return 0;
 if(category==4u||(category==3u&&p[0x510u]==1u))return 0;
 if(category<5u&&type!=category+1u)return 0;
 if(category==5u && (type<6u||type>11u))return 0;
 /* Orders are now governed by our tactical policy on transports and warriors.
    Script-managed, unknown-origin and special ships still fail closed. */
 if(read32(p,0x0cu)||read32(p,0x3ccu) ||
    p[0x458u]||p[0x461u]||p[0x49cu])return 0;
 if(category!=1u && category!=3u && (p[0x44cu]||p[0x43cu]))return 0;
 return 1;
}
static int safe_target(const uint8_t*p){
 uint32_t own,type;
 if(!p)return 0;
 type=p[0x10u];own=p[0x11u];
 if(!type||type>=12u ||!(own<=4u||own==7u))return 0;
 if(read32(p,0x0cu)||p[0x44cu]||p[0x43cu])return 0;
 return 1;
}
/* Only the two native classes capable of a partnership offer are accepted.
   A player has a different VMT; Klissan/dominator/modded subclasses fail closed. */
static uint32_t known_partner_kind(const uint8_t *p){
 if(candidate(p,0u))return 1u;
 if(candidate(p,2u))return 3u;
 return 0u;
}
/* Check current partnership provenance and record one initial positive
   diplomatic/cooperation link when a genuine native partnership is active. */
void FHDNPC_ObservePartnership(void *ship,uint32_t category){
 uint8_t*p=(uint8_t*)ship,*partner;
 FHDProfile mine,other;uint32_t id,pid,pseed,day;
 if(!g_armed || !(category==0u||category==2u) || !candidate(p,category))return;
 if(!read32(p,0x400u))return; /* contract must be active */
 partner=(uint8_t*)(uintptr_t)read32(p,0x3fcu);
 if(!partner || (!known_partner_kind(partner) && !native_player(partner)))return;
 id=read32(p,4u);pid=read32(partner,4u);pseed=read32(partner,0xe0u);day=read32(p,0xecu);
 if(!id||!pid||id==pid||day>1000000u ||
    FHD_ProfileOf(id,&mine)!=1||
    (!native_player(partner)&&FHD_ProfileOf(pid,&other)!=1)){g_contract_skips++;return;}
 if(native_player(partner))g_player_alliances++;
 if(FHD_RelationPeer(id,pid,pseed)==0 && FHD_RelationPeerLastDay(id,pid,pseed)!=day){
   if(FHD_RecordPeer(id,pid,pseed,FHD_EVENT_DIPLOMACY,4u,day)==1)g_contracts++;
 }
}
/* Same real victim, day, and different attackers in the 16-slot existing
   attack-pair window. No scanning all NPCs or generating synthetic events. */
static int partner_attacked_victim(uint32_t victim,uint32_t attacker,uint32_t vs,
                                  uint32_t as,uint32_t day){
 uint32_t idx,i;
 idx=(victim*2654435761u ^ attacker*2246822519u ^ vs ^ as)&(MAX_PAIRS-1u);
 for(i=0;i<16u;i++){
   uint32_t j=(idx+i)&(MAX_PAIRS-1u);
   if(g_pairs[j].victim==victim && g_pairs[j].attacker==attacker &&
      g_pairs[j].vseed==vs && g_pairs[j].aseed==as && g_pairs[j].day==day)return 1;
 }
 return 0;
}
/* Real game event: TShip.ReactToAttack(Attacker), not speculative hostility. */
void FHDNPC_ObserveAttack(void *victim,void *attacker,uint32_t category){
 uint8_t*v=victim,*a=attacker;uint32_t vid,aid,vs,as,day,idx,i,slot=MAX_PAIRS;
 FHDProfile profile;
 if(!g_armed||!candidate(v,category)||!safe_target(a)){g_protected++;return;}
 if(a[0x10u]==1u && !candidate(a,0u) && !native_player(a)){g_protected++;return;}
 vid=read32(v,4);aid=read32(a,4);vs=read32(v,0xe0u);as=read32(a,0xe0u);day=read32(v,0xecu);
 if(!vid||!aid||vid==aid||vid==0xffffffffu||aid==0xffffffffu||day>1000000u){g_protected++;return;}
 /* First-strike evidence must be recorded even before the core has
    registered this NPC. Otherwise a player-initiated attack could be lost. */
 FHDNPC_GrievanceOnAttack(v,a);
 if(FHD_ProfileOf(vid,&profile)!=1){g_target_unavailable++;return;}
 g_native_attacks++;if(native_player(a))g_player_attacks++;
 idx=(vid*2654435761u ^ aid*2246822519u ^ vs ^ as)&(MAX_PAIRS-1u);
 /* Bounded linear probing: inspect all 16 positions for a same-day pair
    BEFORE recycling an older slot. If saturated, drop the new event rather
    than overcount a repeated attack or scan thousands of positions. */
 for(i=0;i<16u;i++){
   uint32_t j=(idx+i)&(MAX_PAIRS-1u);
   if(g_pairs[j].victim==vid&&g_pairs[j].attacker==aid&&
      g_pairs[j].vseed==vs&&g_pairs[j].aseed==as&&g_pairs[j].day==day){
      g_duplicate_attacks++;return;
   }
   if(slot==MAX_PAIRS && (!g_pairs[j].victim || g_pairs[j].day!=day))slot=j;
 }
 if(slot==MAX_PAIRS){g_duplicate_attacks++;return;}
 g_pairs[slot].victim=vid;g_pairs[slot].attacker=aid;
 g_pairs[slot].vseed=vs;g_pairs[slot].aseed=as;g_pairs[slot].day=day;
 if(a[0x10u]==3u && candidate(a,2u)){
   extern void FHDNPC_StationObservePirateAttack(const void*,const void*,uint32_t);
   FHDNPC_StationObservePirateAttack(v,a,day);
 }
 /* Civilian transports/liners/diplomats activate Forsage (+0x3b5) to flee when attacked by a stronger foe. */
 if(v[0x10u]==2u && !read32(v,0x1cu) && !read32(v,0x20u) && !v[0x459u] && read32(v,0x100u)!=0u){
   uint32_t vs_pow=strength100(v),as_pow=strength100(a);
   if(as_pow>vs_pow)v[0x3b5u]=1u;
 }
 /* Publish combat/wanted reason in the Information panel with both ship names. */
 {extern uint32_t FHDNPC_AttackInfoNotice(const void*,const void*,uint32_t,uint32_t,uint32_t);
  extern uint32_t FHDNPC_BountyFor(uint32_t);
  uint32_t bnt=FHDNPC_BountyFor(vid);
  uint32_t reason=4u;
  if(a[0x10u]==4u)reason=1u;
  else if(a[0x10u]==3u)reason=(FHD_RelationPeer(aid,vid,vs)<=-15)?5u:2u;
  else if(bnt>0u)reason=3u;
  (void)FHDNPC_AttackInfoNotice(a,v,day,reason,bnt);
 }
 if(a[0x10u]==3u&&candidate(a,2u)&&v[0x10u]==2u&&v[0x11u]<=4u){
   const uint8_t *star=(const uint8_t *)(uintptr_t)read32(v,0x24u);
   if(star&&read32(a,0x24u)==(uint32_t)(uintptr_t)star&&!v[0x459u])
     FHD_WorldAttack(read32(star,4u),day);
 }

 /* Betrayal is only claimed for an actual recorded attack by an existing
    partner, never inferred from hostile standings or random observations. */
 {uint32_t event=(read32(v,0x3fcu)==(uint32_t)(uintptr_t)a)
       ?FHD_EVENT_BETRAY:FHD_EVENT_ATTACK;
  {uint32_t weight=(event==FHD_EVENT_BETRAY)?1u:
      FHDNPC_AttackMemoryWeight(v[0x10u]==2u?known_transport_profession(v):0u);
   if(FHD_RecordPeer(vid,aid,as,event,weight,day)==1){g_attack_pairs++;
     if(weight>1u)g_attack_weighted++;
     if(event==FHD_EVENT_BETRAY)g_betrayals++;}
  }
 }
 /* Confirmed joint battle: the current attacker AND its contractual partner
    both actually attacked this same ship today.  Both must be managed native
    NPCs; a nearby escort or mere friendship is NOT enough. */
 {uint8_t *partner=(uint8_t*)(uintptr_t)read32(a,0x3fcu);
  FHDProfile ap,bp;uint32_t pid,ps;
  if(known_partner_kind(a) && partner && known_partner_kind(partner) &&
     read32(a,0x400u)>0u){
    pid=read32(partner,4u);ps=read32(partner,0xe0u);
    if(pid && pid!=aid && FHD_ProfileOf(aid,&ap)==1 &&
       FHD_ProfileOf(pid,&bp)==1 &&
       FHDNPC_JointCombatCredit(partner_attacked_victim(vid,pid,vs,ps,day),1,1)){
      if(FHD_RecordPeer(aid,pid,ps,FHD_EVENT_HELP,10u,day)==1)g_joint_fights++;
      /* Credit the partner only if we have both witnessed the same battle. */
      FHD_RecordPeer(pid,aid,as,FHD_EVENT_HELP,10u,day);
    }
  }
  /* Confirmed defense of a third ship under attack by `v`: if `v` currently
     has `v->Enemy == ally` in the same star system and `a` attacks `v`,
     record positive combat assistance between `ally` and `a`. */
  {uint8_t *ally=(uint8_t*)(uintptr_t)read32(v,0x3f4u);
   if(ally && ally!=a && ally!=v && read32(ally,0x24u)==read32(v,0x24u) &&
      !ally[0x459u] && !read32(ally,0x1cu) && !read32(ally,0x20u)){
     uint32_t ally_id=read32(ally,4u),ally_seed=read32(ally,0xe0u);
     if(ally_id && ally_id!=aid && ally_id!=0xffffffffu){
       if(ally[0x10u]>=1u && ally[0x10u]<=4u && candidate(ally,ally[0x10u]-1u) &&
          FHD_ProfileOf(ally_id,&bp)==1 && FHD_RelationPeer(ally_id,aid,as)>=-10){
         if(FHD_RecordPeer(ally_id,aid,as,FHD_EVENT_HELP,3u,day)==1)g_joint_fights++;
       }
       if(a[0x10u]>=1u && a[0x10u]<=4u && candidate(a,a[0x10u]-1u) &&
          (native_player(ally)||(ally[0x10u]>=1u&&ally[0x10u]<=4u&&candidate(ally,ally[0x10u]-1u))) &&
          FHD_ProfileOf(aid,&ap)==1 && FHD_RelationPeer(aid,ally_id,ally_seed)>=-10){
         FHD_RecordPeer(aid,ally_id,ally_seed,FHD_EVENT_HELP,2u,day);
       }
     }
   }
  }
 }
}
/* Return a relationship target only when its native class, exact VMT,
   owner, identity and already-registered profile are confirmed. This is a
   read-only query for the existing live personality calculation; it neither
   changes the target nor sends a native order. Player/alien/modded targets
   are never registered as managed NPCs by this helper. */
uint32_t FHDNPC_VerifiedRelationIdentity(void *self,void *target,uint32_t *seed){
 uint8_t*p=(uint8_t*)self,*t=(uint8_t*)target;
 uint32_t kind,id,eid;
 if(!g_armed||!p||!t){g_target_rejected++;return 0;}
 kind=p[0x10u];
 if(!(kind==1u||kind==3u)||!candidate(p,kind-1u)){
   g_target_rejected++;return 0;
 }
 kind=t[0x10u];
 if(!(kind>=1u && kind<=4u) ||
    (!native_player(t) && !candidate(t,kind-1u))){
   g_target_rejected++;return 0;
 }
 id=read32(p,4u);eid=read32(t,4u);
 *seed=read32(t,0xe0u);
 if(!id||!eid||id==eid||eid==0xffffffffu||
    (!native_player(t)&&FHD_ProfileOfIdentity(eid,*seed,0)!=1)){g_target_rejected++;return 0;}
 g_target_verified++;if(native_player(t))g_player_rel_targets++;
 if(FHD_RelationPeer(id,eid,*seed)!=0)g_target_linked++;
 return eid;
}
API uint32_t FHDNPC_VerifiedRelationTarget(void *self,void *target){
 uint32_t seed;
 return FHDNPC_VerifiedRelationIdentity(self,target,&seed);
}
uint32_t FHDNPC_ObservedThreat(void *self,void *target){
 const uint8_t *p=self,*t=target;
 if(!g_armed||!p||!t||!read32(p,0x24u)||read32(p,0x24u)!=read32(t,0x24u)||
    t[0x459u]||read32(t,0x1cu)||read32(t,0x20u))return 50u;
 uint32_t own=strength100(p),other=strength100(t);
 if(!own||!other)return 50u;
 if(other/2u>=own)return 100u;
 return (uint32_t)((double)other*50.0/(double)own);
}
/* Review newly chosen targets only when the ship is free to select them. */
void FHDNPC_FilterSelectedEnemy(void *ship,uint32_t kind,uint32_t old_enemy){
 uint8_t*p=ship,*enemy;uint32_t category,id,eid,raw_self,raw_enemy,risk=50u;
 uint32_t etype,eowner,dom;
 FHDProfile prof;int32_t rel,score=0,outcome;
 if(!g_armed||kind>2u||!p)return;
 if(kind==2u){
   uint8_t *target;FHDProfile myprofile;uint32_t myid,otherid,motive;
   int32_t relation;
   g_pirate_calls++;
   if(!candidate(p,2u)){g_pirate_unverified++;return;}
   /* Old target is already a fight/pursuit: do not erase revenge or combat. */
   if(old_enemy)return;
   target=(uint8_t*)(uintptr_t)read32(p,0x3f4u);
   if(!target)return;
   if(!native_player(target) &&
      !(target[0x10u]>=1u && target[0x10u]<=4u &&
        candidate(target,target[0x10u]-1u))){g_pirate_unverified++;return;}
   myid=read32(p,4u);otherid=read32(target,4u);
   if(!myid || !otherid || myid==otherid ||
      FHD_ProfileOf(myid,&myprofile)!=1){g_pirate_unverified++;return;}
   relation=FHD_RelationPeer(myid,otherid,read32(target,0xe0u));
   /* TShip.CargoGoods is a verified eight-element packed array at 0x30;
      do not mistake lack of cash for lack of valuable cargo. */
   {uint32_t c,has_cargo=0;
    for(c=0;c<8u;c++){
      int32_t quantity=(int32_t)read32(target,0x30u+16u*c);
      if(quantity>0 && quantity<1000000){has_cargo=1;break;}
    }
    motive=FHDNPC_PirateMotive(&myprofile,relation,p[0x514u],
      (int32_t)read32(target,0xf0u),has_cargo,0u,native_player(target));
   }
   if(motive==FHD_PIRATE_RANSOM_OR_CARGO){
     uint32_t ownb=read32(p,0xb8u),enb=read32(target,0xb8u);
     if(ownb && enb && !(ownb&0x80000000u) && !(enb&0x80000000u) &&
        (ownb&0x7f800000u)!=0x7f800000u &&
        (enb&0x7f800000u)!=0x7f800000u){
       float own,enemy;uint32_t i,ou,eu;
       {uint8_t*o=(uint8_t*)&own,*e=(uint8_t*)&enemy;
        for(i=0;i<4u;i++){o[i]=((uint8_t*)&ownb)[i];e[i]=((uint8_t*)&enb)[i];}}
       if(own>0.01f&&enemy>0.01f&&own<1000000.0f&&enemy<1000000.0f){
        ou=(uint32_t)(own*100.0f);eu=(uint32_t)(enemy*100.0f);
        if(FHDNPC_AvoidStrongPrey(&myprofile,motive,ou,eu)){
          motive=FHD_PIRATE_NO_VERIFIED_MOTIVE;g_pirate_smart_avoid++;
        }
       }
     }
   }
   if(motive==FHD_PIRATE_REVENGE)g_pirate_revenge++;
   else if(motive==FHD_PIRATE_RANSOM_OR_CARGO)g_pirate_economic++;
   else if(motive==FHD_PIRATE_CLAN_DUTY)g_pirate_clan++;
   else if(motive==FHD_PIRATE_PROTECT_FRIEND)g_pirate_spare++;
   if(motive==FHD_PIRATE_NO_VERIFIED_MOTIVE ||
      motive==FHD_PIRATE_PROTECT_FRIEND){
      /* Do not cancel an already issued mandatory order or an active defense. */
      if(!p[0x3b4u] && !p[0x44cu] && !p[0x43cu] &&
          p[0x428u]!=6u && !read32(p,0x430u)){
        *(volatile uint32_t*)(p+0x3f4u)=0u;g_pirate_veto++;
      }
   }
   return;
 }
 g_select_calls++;g_select_kind[kind][0]++;
 category=kind==0u?1u:3u;
 if(!candidate(p,category)){g_protected++;return;}
 if(old_enemy){g_existing_target++;g_select_kind[kind][1]++;g_override_existing[kind]++;return;}
 if(p[0x3b4u]){g_self_defense++;g_override_fear[kind]++;return;}
 if(kind==1u && p[0x510u]==1u){g_flagship_protected++;g_override_flagship[kind]++;return;}
 if(read32(p,0x3fcu)||read32(p,0x430u)||read32(p,0x3d0u)||p[0x43cu]||p[0x44cu]){
   g_order_protected++;g_override_order[kind]++;return;
 }
 enemy=(uint8_t*)(uintptr_t)read32(p,0x3f4u);
 if(!enemy){g_no_new_target++;g_select_kind[kind][2]++;return;}
 etype=enemy[0x10u];eowner=enemy[0x11u];
 dom=etype==0u && eowner==5u;
 /* Interfering with a different protected actor's AI is forbidden.  A
    dominator may be *targeted* by a transport/warrior, without our code ever
    modifying the dominator object or its own decisions. Its target risk is
    evaluated, never its memories. All other targets must have exact native VMT. */
 if(!dom && !native_player(enemy) &&
    !(etype>=1u && etype<=4u && candidate(enemy,etype-1u))){
   g_target_protected++;g_select_kind[kind][4]++;return;
 }
 id=read32(p,4);eid=read32(enemy,4);
 if(!id||id==0xffffffffu||!eid||eid==0xffffffffu||eid==id){
   g_target_protected++;g_select_kind[kind][4]++;return;
 }
 if(FHD_ProfileOf(id,&prof)!=1){g_profile_missing++;return;}
 rel=dom?0:FHD_RelationPeer(id,eid,read32(enemy,0xe0u));
 /* TShip.Strength (Single) is cached by the game. Invalid data uses neutral
    risk, not speculative calculations or unsafe calls into the game engine. */
 raw_self=read32(p,0xb8u);raw_enemy=read32(enemy,0xb8u);
 if((raw_self&0x7f800000u)==0x7f800000u ||
    (raw_enemy&0x7f800000u)==0x7f800000u ||
    (raw_self&0x80000000u)||(raw_enemy&0x80000000u)||
    !raw_self||!raw_enemy){g_strength_unknown++;}
 else {
   float own,other,ratio;uint32_t computed;
   {uint32_t i;uint8_t*od=(uint8_t*)&own,*ed=(uint8_t*)&other;
    for(i=0;i<4u;i++){od[i]=((uint8_t*)&raw_self)[i];ed[i]=((uint8_t*)&raw_enemy)[i];}}
   if(own>0.0001f && other>0.0001f && own<1.0e30f && other<1.0e30f){
     ratio=other/own;
     if(ratio>10.0f)risk=100u;
     else if(ratio<0.1f)risk=4u;
     else {computed=(uint32_t)(ratio*38.0f);risk=computed>100u?100u:computed;}
   }else g_strength_unknown++;
 }
 outcome=FHDNPC_AssessEnemy(&prof,rel,kind,dom,risk,&score);
 (void)score;g_filtered_eligible++;g_select_kind[kind][5]++;
 if(dom)g_override_dominator[kind]++;
 if(outcome!=FHD_TACTICAL_ALLOW){
   /* Neither OrderTarget nor Order fields are touched.  Suppression can now
      occur even for a previously selected enemy and mandatory native orders. */
   *(volatile uint32_t*)(p+0x3f4u)=0u;
   if(kind==0u)g_transport_avoided++;else g_warrior_avoided++;
   if(outcome==FHD_TACTICAL_AVOID_FRIEND)g_friend_avoided++;
   else g_risk_avoided++;
 }else {g_policy_allowed++;g_no_decision[kind]++;}
}
static int change(void **slot,void*expected,void*value){uint32_t old=0,unused=0;
 if(!VirtualProtect(slot,4u,EXEC_RW,&old))return 0;
 if(*slot!=expected){VirtualProtect(slot,4u,old,&unused);return 0;}
 *(volatile void**)slot=value;
 VirtualProtect(slot,4u,old,&unused);return 1;}
API int32_t FHDNPC_DisarmSocial(void){uint8_t*base=(uint8_t*)g_base;uint32_t i;
 if(!g_armed||!base)return 0;
 for(i=0;i<ENTRY_COUNT;i++){
  if(i==TRANCLUCATOR_CATEGORY)continue;
  void**slot=(void**)(base+slot_rva[i]);
  if(*slot==stub_ptr[i])change(slot,stub_ptr[i],*orig_ptr[i]);
 }
 g_armed=0;return 1;
}
API int32_t FHDNPC_ArmSocial(void){
 uint8_t*base=(uint8_t*)GetModuleHandleW(0);uint32_t i,j;
 if(g_armed)return 1;
 if(!base)return 0;
 for(i=0;i<ATTACK_COUNT;i++){
  slot_rva[i]=base_vmt_next[i]+0x60u;orig_rva[i]=attack_rva[i];
 }
 slot_rva[6]=base_vmt_next[1]+0x50u;orig_rva[6]=select_rva[0];
 slot_rva[7]=base_vmt_next[3]+0x50u;orig_rva[7]=select_rva[1];
 slot_rva[8]=base_vmt_next[2]+0x50u;orig_rva[8]=select_rva[2];
 slot_rva[9]=base_vmt_next[1]+0x68u;orig_rva[9]=0x1ad6f0u;
 slot_rva[10]=base_vmt_next[1]+0x70u;orig_rva[10]=0x1add20u;
 slot_rva[11]=base_vmt_next[1]+0x88u;orig_rva[11]=0x1ae678u;
 for(i=0;i<ENTRY_COUNT;i++){
  if(i==TRANCLUCATOR_CATEGORY)continue;
  uint8_t *f=base+orig_rva[i];
  if(*(void**)(base+slot_rva[i])!=(void*)f)return 0;
  /* A small signature guard for the specific method from supplied executable. */
  if(f[0]!=0x53u && f[0]!=0x55u && f[0]!=0x56u && f[0]!=0x8bu)return 0;
 }
 for(i=0;i<ENTRY_COUNT;i++)*orig_ptr[i]=(void*)(base+orig_rva[i]);
 for(i=0;i<ENTRY_COUNT;i++){
  if(i==TRANCLUCATOR_CATEGORY)continue;
  if(!change((void**)(base+slot_rva[i]),*orig_ptr[i],stub_ptr[i])){
    for(j=0;j<i;j++)if(j!=TRANCLUCATOR_CATEGORY)
      change((void**)(base+slot_rva[j]),stub_ptr[j],*orig_ptr[j]);
    return 0;
  }
 }
 g_base=(uintptr_t)base;g_armed=1;return 1;
}
API void FHDNPC_ResetSocial(void){uint32_t i;
 g_native_attacks=0;g_attack_pairs=0;g_transport_avoided=0;g_warrior_avoided=0;
 g_filtered_eligible=0;g_protected=0;g_target_unavailable=0;g_duplicate_attacks=0;
 g_select_calls=0;g_existing_target=0;g_no_new_target=0;g_flagship_protected=0;
 g_order_protected=0;g_self_defense=0;g_target_protected=0;g_profile_missing=0;
 g_friend_avoided=0;g_risk_avoided=0;g_policy_allowed=0;g_strength_unknown=0;
 g_betrayals=0;
 g_player_attacks=g_player_rel_targets=g_player_alliances=0;
 g_pirate_calls=g_pirate_economic=g_pirate_revenge=0;
 g_pirate_clan=g_pirate_spare=g_pirate_veto=g_pirate_unverified=0;
 g_pirate_smart_avoid=0;
 g_hop_fear_calls=g_hop_changes=g_hop_interrupted=g_hop_unsafe=0;
 g_transport_fear_checks=g_transport_memory_fear=g_transport_against_player=0;
 g_transport_trust_checks=g_transport_trust_refused=0;
 g_transport_extortion_events=g_transport_extortion_player=0;
 g_transport_memory_unknown=g_attack_weighted=0;
 for(i=0;i<2u;i++){
   g_override_order[i]=g_override_existing[i]=g_override_flagship[i]=0;
   g_override_fear[i]=g_override_dominator[i]=g_no_decision[i]=0;
 }
 g_contracts=0;g_joint_fights=0;g_contract_skips=0;
 g_escorts=0;g_escort_day_skips=0;g_escorts_ineligible=0;
 g_target_verified=0;g_target_linked=0;g_target_rejected=0;
 for(i=0;i<2u;i++){uint32_t j;for(j=0;j<6u;j++)g_select_kind[i][j]=0;}
 for(i=0;i<MAX_PAIRS;i++)g_pairs[i].victim=0;
}
API void FHDNPC_SocialStats(uint32_t*out){
 if(!out)return;
 out[0]=g_native_attacks;out[1]=g_attack_pairs;
 out[2]=g_filtered_eligible;out[3]=g_transport_avoided;
 out[4]=g_warrior_avoided;out[5]=g_duplicate_attacks;
 out[6]=g_protected;out[7]=g_target_unavailable;
 out[8]=g_select_calls;out[9]=g_existing_target;out[10]=g_no_new_target;
 out[11]=g_flagship_protected;out[12]=g_order_protected;
 out[13]=g_self_defense;out[14]=g_target_protected;
 out[15]=g_profile_missing;out[16]=g_friend_avoided;
 out[17]=g_risk_avoided;out[18]=g_policy_allowed;
 out[19]=g_strength_unknown;out[20]=g_betrayals;
 out[21]=g_contracts;out[22]=g_joint_fights;out[23]=g_contract_skips;
 {uint32_t k,j;for(k=0;k<2u;k++)for(j=0;j<6u;j++)out[24u+k*6u+j]=g_select_kind[k][j];}
 out[36]=g_escorts;out[37]=g_escort_day_skips;out[38]=g_escorts_ineligible;
 out[39]=g_target_verified;out[40]=g_target_linked;out[41]=g_target_rejected;
}

/* Extra diagnostic API without altering the existing 42-DWORD ABI. */
API void FHDNPC_Social140Stats(uint32_t*out){
 uint32_t k;
 if(!out)return;
 for(k=0;k<2u;k++){
   out[k*6u+0u]=g_override_order[k];
   out[k*6u+1u]=g_override_existing[k];
   out[k*6u+2u]=g_override_flagship[k];
   out[k*6u+3u]=g_override_fear[k];
   out[k*6u+4u]=g_override_dominator[k];
   out[k*6u+5u]=g_no_decision[k];
 }
}

/* 1.5 additive stats; no change to existing ABI or SAV. */
API void FHDNPC_Social150Stats(uint32_t*out){
 if(!out)return;
 out[0]=g_player_attacks;out[1]=g_player_rel_targets;out[2]=g_player_alliances;
 out[3]=g_pirate_calls;out[4]=g_pirate_economic;out[5]=g_pirate_revenge;
 out[6]=g_pirate_clan;out[7]=g_pirate_spare;out[8]=g_pirate_veto;
 out[9]=g_pirate_unverified;
 out[14]=g_pirate_smart_avoid;
 out[10]=g_hop_fear_calls;out[11]=g_hop_changes;
 out[12]=g_hop_interrupted;out[13]=g_hop_unsafe;
}

/* New 1.6 diagnostics, independent of 1.5 public 15-DWORD array and SAV. */
API void FHDNPC_Social160Stats(uint32_t*out){
 if(!out)return;
 out[0]=g_transport_fear_checks;out[1]=g_transport_memory_fear;
 out[2]=g_transport_against_player;out[3]=g_transport_memory_unknown;
 out[4]=g_transport_trust_checks;out[5]=g_transport_trust_refused;
 out[6]=g_transport_extortion_events;out[7]=g_transport_extortion_player;
 out[8]=g_attack_weighted;
}
