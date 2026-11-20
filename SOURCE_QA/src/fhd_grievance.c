/* FHD NPC AI 2.2.5. Exact 2026 Rangers.exe only.
   Provenance: player/ordinary NPC ReactToAttack, TStar.CompleteItemPickup,
   TShip.FireWeaponAtItem, TNormalShip.ProcessShipKill.
   Non-VMT inline patches are signature-verified and optional. No changed SAV ABI.
*/
#include <stdint.h>
#include "fhd_grievance.h"
#include "fhd_grievance_rules.h"
#include "fhd_dialogue.h"
#include "fhd_profession_policy.h"
#if defined(FHD_GRIEVANCE_TEST) && defined(__i386__)
#define API
#define IMP
#define ST
#elif defined(_WIN32) && defined(_M_IX86)
#define API __declspec(dllexport)
#define IMP __declspec(dllimport)
#define ST __stdcall
#else
#error Windows x86 native bridge required
#endif
IMP void *ST GetModuleHandleW(const uint16_t *);
IMP uint16_t *ST SysAllocStringLen(const uint16_t *,uint32_t);
IMP void ST SysFreeString(uint16_t *);
IMP int ST VirtualProtect(void*,uint32_t,uint32_t,uint32_t*);
IMP void *ST GetCurrentProcess(void);
IMP int ST FlushInstructionCache(void*,const void*,uint32_t);
#define PROT 0x40u
#define CONFLICT_SLOTS 2048u
#define OFFER_SLOTS 256u
#define MAX_SCAN_SHIPS 4096u
#define PLAYER_VMT_RVA 0x347134u
#define RANGER_REACT_RVA 0x316f88u
#define PLAYER_REACT_SLOT (PLAYER_VMT_RVA+0x60u)
#define PLAYER_VMT_PTR (PLAYER_VMT_RVA-0x1cu)
/* GetPlayer at 0x00747300 decodes EncodedPlayer at 0x008100fc.
   TPlayer inherits TRanger but has its OWN VMT: comparing with the
   inherited TRanger VMT silently classifies all player loot as NPC loot. */
#define ENCODED_PLAYER_RVA 0x4100fcu
#define PLAYER_XOR_KEY 0xb1cd15d3u
#define REPUTATION_RVA 0x317848u
#define PICKUP_RVA 0x3f4f5cu
#define SHOT_RVA 0x2d1b2cu
#define KILL_RVA 0x33fc64u
#define HOME_RVA 0x317c89u
#define HOME_END_RVA 0x317cd5u
#define PLANET_CHANGE_REL_RVA 0x339430u
#define SET_MONEY_RVA 0x2c7938u
#define TRY_EXTORT_RVA 0x2d4390u /* Kept for existing vanilla extortion; unrelated NPC demands. */
#define MONEY_RESPONSE_RVA 0x31a298u /* TRanger.BuildMoneyExtortionResponse (inherited by TPlayer). */
/* Native dialogue gates from aGalaxy/aCalc/GlobalsV (2026 EXE only). */
#define EXTORT_COOLDOWN_RVA 0x413090u
#define DIALOGUE_COUNT_RVA 0x41d2a8u
#define CURRENT_SCREEN_RVA 0x40a93cu
#define TURN_PHASE_RVA 0x41ae00u
#define EXIT_SCREEN_RVA 0x412f7cu
#define AUTO_PLAYER_RVA 0x40ffa4u
#define CAN_CONTACT_RVA 0x2eba6cu
#define ORDER_MOVE_RVA 0x2e2560u
#define JOINT_RVA 0x2d3ec8u
#define MISSILE_MINERALS_RVA 0x3f7f0bu
#define MISSILE_ITEM_RVA 0x3f8295u
#define RELATION_LEVEL_CALL_RVA 0x2d42a4u
#define RELATION_VALUE_RVA 0x2f3664u
#define APPROACH_RVA 0x2e237cu
#define CLEAR_ITEM_RVA 0x3e145cu
#define GALAXY_RVA 0x41d2a0u
#define FORM_INFO_RVA 0x101a6au
extern void FHDNPC_PlayerReactStub(void);
extern void FHDNPC_ReputationStub(void);
extern void FHDNPC_PickupStub(void);
extern void FHDNPC_ShotStub(void);
extern void FHDNPC_KillStub(void);
extern void FHDNPC_ReputationHomeStub(void);
extern void FHDNPC_MissileLootStub(void);
extern void FHDNPC_RelationLevelStub(void);
extern void FHDNPC_RelationValueStub(void);
extern void FHDNPC_RelationRawTrampoline(void);
extern void FHDNPC_FormInfoStub(void);
extern void FHDNPC_InvokeJoint(void*,void*,void*);
extern void FHDNPC_InvokeSetMoney(void*,uint32_t);
extern void FHDNPC_InvokePlanetRelation(void*,void*,int32_t);
extern uint32_t FHDNPC_InvokeExtort(void*,void*,void*);
extern uint32_t FHDNPC_InvokeMoneyDemand(void*,void*,void*,uint16_t**,uint32_t);
extern uint32_t FHDNPC_Profession(uint32_t,uint32_t);
extern uint32_t FHDNPC_LookupVerifiedProfession(uint32_t,uint32_t,uint32_t);
void *fhd_player_react_orig;
void *fhd_reputation_continue;
void *fhd_pickup_continue;
void *fhd_shot_continue;
void *fhd_kill_continue;
void *fhd_home_continue;
void *fhd_home_end;
void *fhd_set_money;
void *fhd_joint_attack;
void *fhd_planet_relation;
void *fhd_clear_item;
void *fhd_relation_value;
void *fhd_relation_continue;
void *fhd_forminfo_continue;
static uintptr_t g_base;
static uint32_t g_armed;
enum { REQUIRED_SITES=5, SITE_COUNT=10 };
static const uint32_t k_rva[SITE_COUNT]={REPUTATION_RVA,PICKUP_RVA,SHOT_RVA,KILL_RVA,HOME_RVA,
                                          MISSILE_MINERALS_RVA,MISSILE_ITEM_RVA,RELATION_LEVEL_CALL_RVA,
                                          RELATION_VALUE_RVA,FORM_INFO_RVA};
static const uint32_t k_len[SITE_COUNT]={6u,5u,6u,8u,5u,5u,5u,5u,6u,9u};
static void *const k_stubs[SITE_COUNT]={FHDNPC_ReputationStub,FHDNPC_PickupStub,
  FHDNPC_ShotStub,FHDNPC_KillStub,FHDNPC_ReputationHomeStub,FHDNPC_MissileLootStub,
  FHDNPC_MissileLootStub,FHDNPC_RelationLevelStub,FHDNPC_RelationValueStub,FHDNPC_FormInfoStub};
static const uint8_t k_sig[SITE_COUNT][9]={
 {0x55,0x8b,0xec,0x83,0xc4,0xdc},
 {0x55,0x8b,0xec,0x51,0x53},
 {0x55,0x8b,0xec,0x83,0xc4,0xc4},
 {0x55,0x8b,0xec,0xb9,0x0e,0,0,0},
 {0x8b,0x5f,0x2c,0x85,0xdb},
 {0xe8,0x4c,0x95,0xfe,0xff},
 {0xe8,0xc2,0x91,0xfe,0xff},
 {0xe8,0xbb,0xf3,0x01,0x00},
 {0x53,0x56,0x57,0x55,0x8b,0xfa},
 {0x8b,0x45,0xf4,0x8b,0x15,0x98,0x12,0x5b,0x00}};
static uint8_t optional_installed[SITE_COUNT-REQUIRED_SITES];
static struct {uint32_t id,seed,item,day,kind;} g_loot_seen[CONFLICT_SLOTS];
/* Recovered native-response contract tables. No persistence to SAV layout. */
static struct {uint32_t issuer,iseed,target,tseed,executor,eseed,amount,day,issued,paid,star;} g_offers[OFFER_SLOTS];
static struct {uint32_t issuer,iseed,target,tseed,star,day,first_day;} g_grudges[CONFLICT_SLOTS];
static struct {uint32_t vid,vseed,pseed,star,day,defense,suppress_home,valid;} g_last_kill_defense;
/* A request is queued ONLY for the witnessed, same-system player conflict.
   It is executed on the ordinary NPC's native AI tick, where the original
   game can safely pause the star map and open the existing conversation. */
#define CLAIM_SLOTS 128u
static struct {uint32_t issuer,seed,player_id,player_seed,star,day,item,kind,cost,amount;uint32_t active,last_attempt,pre_rel;} g_claims[CLAIM_SLOTS];
static uint32_t g_claim_results[4]; /* attempted,accepted,request_count_increased,deferred */
static uint32_t g_stat[20];
static uint32_t g_pirate_retaliations;
static uint32_t g_claim_block[6];
static uint32_t read32(const void *x,uint32_t offset){
 const uint8_t *p=(const uint8_t*)x+offset;
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int player(const uint8_t*p){
 /* Match the CURRENT live player by object identity, not by TRanger VMT.
    The EXE's verified GetPlayer returns EncodedPlayer xor PLAYER_XOR_KEY.
    No other pilot, including other rangers, can match this unique object. */
 if(!p||!g_base)return 0;
 uint32_t live=read32((const void*)g_base,ENCODED_PLAYER_RVA)^PLAYER_XOR_KEY;
 return live && (uint32_t)(uintptr_t)p==live &&
        p[0x10u]==1u && read32(p,4u)!=0u;
}
/* Native-class Coalition/Pirate ships including custom-type / mod-scripted roles
   such as Special Agents, Freelancers, and Explorers (CustomShipType at +0x0c,
   Script at +0x3cc). Strictly excludes Dominators, Klissans, Flagships, and Player. */
static int loot_actor(const uint8_t*p){
 uint32_t type,owner;
 static const uint32_t native_rva[4]={0x310c8cu,0x1ab5a8u,0x1992ccu,0x1a2b2cu};
 if(!g_base||!p||player(p))return 0;
 type=p[0x10u];owner=p[0x11u];
 if(type<1u||type>4u||(type==4u&&p[0x510u]==1u)||!(owner<=4u||owner==7u)||
   read32(p,0u)!=(uint32_t)(g_base+native_rva[type-1u]-0x1cu) ||
   !read32(p,4u)||read32(p,4u)==0xffffffffu||
   p[0x458u]||p[0x461u]||p[0x49cu])return 0;
 return 1;
}
static int ordinary(const uint8_t*p){
 if(!loot_actor(p))return 0;
 if(read32(p,0x0cu)||read32(p,0x3ccu))return 0;
 return 1;
}
/* Delphi TShip.RelativeStrength is a 32-bit IEEE-754 Single, NOT integer.
   Use a bounded numeric conversion for budget and aggression calculations. */
static uint32_t numeric_strength(const uint8_t*p){
 float value;uint32_t bits=read32(p,0xb8u),i;
 uint8_t*d=(uint8_t*)&value;
 if((bits&0x80000000u)||(bits&0x7f800000u)==0x7f800000u||!bits)return 0;
 for(i=0;i<4u;i++)d[i]=(uint8_t)(bits>>(8u*i));
 if(value<0.01f||value>1000000.0f)return 0;
 return (uint32_t)(value*20.0f)+1u;
}
static uint32_t star_id(const uint8_t *ship);
static uint32_t hash_claim(uint32_t id,uint32_t seed){
 return ((id*2654435761u)^seed*2246822519u)&(CLAIM_SLOTS-1u);
}
/* TItem.Cost is the WHOLE stack's game cost (TGoods.Quantity is already
   reflected in Cost). The 2x demand is money, not a per-ton multiplier. */
static uint32_t restitution_amount(uint32_t item_cost){
 if((int32_t)item_cost<=0)return 0u;
 return item_cost>0x3fffffffu?0x7fffffffu:item_cost*2u;
}
static int should_demand_compensation(const FHDProfile *prof,uint32_t kind,uint32_t cost,
                                      int32_t effective_rel,int32_t prior,int player_stronger){
 if(!prof||!cost)return 0;
 /* Friends / Good attitude pilots forgive a first-time loot incident with just a warning. */
 if(effective_rel>=60 && prior>=0)return 0;
 /* Peaceful liners/diplomats or cautious weaker pilots do not confront the player with a bill. */
 if(prof->role==FHD_ROLE_LINER||prof->role==FHD_ROLE_DIPLOMAT)return 0;
 if(player_stronger && prof->traits[3]>=60u && prof->traits[2]<50u && prior>=0)return 0;
 /* Repeat offense always triggers a restitution demand. */
 if(prior<0)return 1;
 /* Greedy, vengeful, pirate, magnate or mercenary personalities demand payment if not intimidated. */
 if(prof->character==FHD_BANDIT||prof->character==FHD_CORSAIR||prof->character==FHD_RAIDER||
    prof->character==FHD_DEALER||prof->character==FHD_MAGNATE||prof->character==FHD_MERCENARY||
    prof->role==FHD_ROLE_PIRATE||prof->role==FHD_ROLE_MERCENARY||
    (uint32_t)prof->traits[2]+(uint32_t)prof->traits[6]>=115u){
   if(!player_stronger || prof->traits[0]>=50u)return 1;
 }
 /* Destroying a high-value item still prompts a demand from a bold pilot. */
 if(kind==2u && cost>=1500u && prof->traits[0]>=40u)return 1;
 return 0;
}
static uint32_t g_pending_pre_rel;
static void queue_claim(const uint8_t *observer,const uint8_t *actor,uint32_t item,uint32_t day,uint32_t kind,uint32_t cost){
 uint32_t demand=restitution_amount(cost);
 if(!demand)return; /* Missing or nonpositive prices must not create fake charges. */
 uint32_t slot=hash_claim(read32(observer,4u),read32(observer,0xe0u));
 if(g_claims[slot].active && g_claims[slot].issuer==read32(observer,4u) &&
    g_claims[slot].seed==read32(observer,0xe0u) &&
    g_claims[slot].day==day){
    /* Several separate item stacks lost to the same player on one day:
       aggregate prices into ONE native dialog, never send duplicate claims. */
    uint32_t old=g_claims[slot].cost;
    g_claims[slot].cost=cost>0x7fffffffu-old?0x7fffffffu:old+cost;
    g_claims[slot].amount=restitution_amount(g_claims[slot].cost);
    return;
 }
 g_claims[slot].issuer=read32(observer,4u);
 g_claims[slot].seed=read32(observer,0xe0u);
 g_claims[slot].player_id=read32(actor,4u);
 g_claims[slot].player_seed=read32(actor,0xe0u);
 g_claims[slot].star=star_id(observer);g_claims[slot].day=day;
 g_claims[slot].item=item;g_claims[slot].kind=kind;
 g_claims[slot].cost=cost;g_claims[slot].amount=demand;
 g_claims[slot].active=1;g_claims[slot].last_attempt=0u;g_claims[slot].pre_rel=g_pending_pre_rel;
 g_stat[7]++; /* real compensation request queued, not just rhetorical protest */
}
static uint32_t world_day(void){
 const uint8_t *world=g_base?(const uint8_t*)(uintptr_t)read32((const void*)g_base,GALAXY_RVA):0;
 return world?read32(world,0x4cu):0u;
}
static int same_star(const uint8_t*a,const uint8_t*b){
 return read32(a,0x24u)&&read32(a,0x24u)==read32(b,0x24u);
}
static uint32_t star_id(const uint8_t *ship){
 const void *star=(const void*)(uintptr_t)read32(ship,0x24u);
 return star?read32(star,4u):0;
}
typedef int (__attribute__((regparm(3))) *RadarRange)(const uint8_t *);
static int saw_item(const uint8_t *ship,const uint8_t *item){
  int range=((RadarRange)(g_base+0x2d7288u))(ship);
  if(range<=0)return 0;
  /* Positions stored as IEEE floats in the witnessed star. */
  union {float value;uint32_t bits;} x,y;
  x.bits=read32(ship,0x14u);
  y.bits=read32(ship,0x18u);
  union {float value;uint32_t bits;} ix,iy;
  ix.bits=read32(item,0x10u);
  iy.bits=read32(item,0x14u);
  float dx=x.value-ix.value,dy=y.value-iy.value,r=(float)range;
  return dx*dx+dy*dy<=r*r;
}
static int ensure_captain(const uint8_t *other){
 uint32_t id=read32(other,4u),seed=read32(other,0xe0u);
 if(!FHD_ProfileOfIdentity(id,seed,0)){
   uint32_t type=other[0x10u],career=FHD_ROLE_UNKNOWN,pr=0,tr=0,mi=0;
   if(type==1u){career=FHD_ROLE_RANGER;tr=other[0x513u];pr=other[0x514u];mi=other[0x515u];}
   else if(type==2u){career=FHDNPC_RoleFromProfession(FHDNPC_Profession(type,other[0x510u]));tr=90;mi=10;}
   else if(type==3u){career=FHD_ROLE_PIRATE;pr=90;mi=10;}
   else {career=FHD_ROLE_MILITARY;mi=90;tr=10;}
   return FHD_Upsert(id,seed,-1,career,type,0,pr,tr,mi)==1;
 }
 return 1;
}
static void first_contact(const uint8_t *other,const uint8_t *p,uint32_t first){
 if(!ordinary(other)||!p||!ensure_captain(other))return;
 uint32_t id=read32(other,4u),seed=read32(other,0xe0u);
 FHD_NoteFirstStrike(id,seed,read32(p,0xe0u),star_id(other),world_day(),first);
}
void FHDNPC_ObservePlayerAttacked(void*victim,void*attacker){
 const uint8_t*p=(const uint8_t*)victim,*a=(const uint8_t*)attacker;
 if(!g_armed||!player(p)||!ordinary(a)||!same_star(p,a))return;
 first_contact(a,p,1u);g_stat[0]++;
}
/* Only direct, native ReactToAttack events create revenge eligibility.
   Loot-only grievances still lower personal trust, never commission a kill. */
static void track_grudge(const uint8_t*v,const uint8_t*a){
 uint32_t i,id,aid,vseed,aseed,st,today;
 if(!g_armed||!ordinary(v)||!a||!same_star(v,a))return;
 if(!player(a)&&!ordinary(a))return;
 id=read32(v,4u);aid=read32(a,4u);
 vseed=read32(v,0xe0u);aseed=read32(a,0xe0u);
 st=star_id(v);today=world_day();
 i=(id*16777619u^vseed)&(CONFLICT_SLOTS-1u);
 if(g_grudges[i].issuer==id && g_grudges[i].iseed==vseed &&
    g_grudges[i].target==aid && g_grudges[i].tseed==aseed &&
    g_grudges[i].star==st && today>=g_grudges[i].day && today-g_grudges[i].day<=30u){
   g_grudges[i].day=today;
   return;
 }
 g_grudges[i].issuer=id;g_grudges[i].iseed=vseed;
 g_grudges[i].target=aid;g_grudges[i].tseed=aseed;
 g_grudges[i].star=st;g_grudges[i].day=today;g_grudges[i].first_day=today;
}
static int grudge_started_before_today(const uint8_t *v,const uint8_t *a,uint32_t today){
 uint32_t id=read32(v,4u),vseed=read32(v,0xe0u),aid=read32(a,4u),aseed=read32(a,0xe0u);
 uint32_t i=(id*16777619u^vseed)&(CONFLICT_SLOTS-1u);
 return g_grudges[i].issuer==id && g_grudges[i].iseed==vseed &&
        g_grudges[i].target==aid && g_grudges[i].tseed==aseed &&
        g_grudges[i].star==star_id(v) && today>g_grudges[i].first_day;
}
static uint32_t verified_defense(const uint8_t*p,const uint8_t*v){
 if(!g_armed||!player(p)||!ordinary(v)||!same_star(p,v))return 0;
 uint32_t vid=read32(v,4u),vseed=read32(v,0xe0u),pseed=read32(p,0xe0u);
 uint32_t st=star_id(v),day=world_day();
 uint32_t first=FHD_FirstStrike(vid,vseed,pseed,st,day);
 if(!first && g_last_kill_defense.valid &&
    g_last_kill_defense.vid==vid && g_last_kill_defense.vseed==vseed &&
    g_last_kill_defense.pseed==pseed && g_last_kill_defense.star==st &&
    g_last_kill_defense.day==day){
   return g_last_kill_defense.defense;
 }
 return (uint32_t)FHDNPC_JustifiedDefense(first,1u,0u,
               read32(p,0x3f8u)==(uint32_t)(uintptr_t)v||
               read32(v,0x3f8u)==(uint32_t)(uintptr_t)p);
}
/* Outlaws (pirates and smugglers) are outside Coalition law: Coalition planets
   never penalize the player for attacking or destroying them. */
static int is_outlaw_victim(const uint8_t *v){
 FHDProfile prof;
 if(!v)return 0;
 if(v[0x10u]==3u||v[0x11u]==7u)return 1;
 if(FHD_ProfileOfIdentity(read32(v,4u),read32(v,0xe0u),&prof)==1 &&
    (prof.role==FHD_ROLE_PIRATE||prof.role==FHD_ROLE_SMUGGLER))return 1;
 return 0;
}
/* Official state/civilian passenger vessels: TWarrior (type 4) or TTransport
   Passenger Liner (SubType 1) / Diplomat (SubType 2). */
static int is_state_vessel(const uint8_t *v){
 if(!v||v[0x11u]>4u)return 0;
 return v[0x10u]==4u || (v[0x10u]==2u && (v[0x510u]==1u||v[0x510u]==2u));
}
static int station_ship(const uint8_t *p){
 if(!g_base||!p||player(p))return 0;
 uint32_t type=p[0x10u],owner=p[0x11u];
 if(type<6u||type>11u||!(owner<=4u||owner==7u))return 0;
 if(read32(p,0u)!=(uint32_t)(g_base+0x29a3c4u-0x1cu)||!read32(p,4u))return 0;
 return !read32(p,0x1cu)&&!read32(p,0x20u)&&!p[0x459u];
}
static int saw_ship(const uint8_t *observer,const uint8_t *target){
 if(!observer||!target||observer==target||!same_star(observer,target))return 0;
 if(read32(observer,0x1cu)||read32(observer,0x20u)||observer[0x459u])return 0;
 int range=((RadarRange)(g_base+0x2d7288u))(observer);
 if(range<=0)return 0;
 union {float value;uint32_t bits;} ox,oy,tx,ty;
 ox.bits=read32(observer,0x14u);oy.bits=read32(observer,0x18u);
 tx.bits=read32(target,0x14u);ty.bits=read32(target,0x18u);
 float dx=ox.value-tx.value,dy=oy.value-ty.value,r=(float)range;
 return dx*dx+dy*dy<=r*r;
}
/* Only pilots in the SAME star system who have a personal bond with the victim
   (active partner, escort/client contract, personal friend >= 15 in social memory,
   or same pirate clan) react unconditionally in that system — never unrelated ships
   just because they belong to the same race. */
static int bonded_witness(const uint8_t *ally,const uint8_t *v){
 if(!ally||!v||ally==v||!ordinary(ally)||!ordinary(v)||!same_star(ally,v))return 0;
 if(read32(ally,0x1cu)||read32(ally,0x20u)||ally[0x459u])return 0;
 if(read32(ally,0x3fcu)==(uint32_t)(uintptr_t)v||
    read32(v,0x3fcu)==(uint32_t)(uintptr_t)ally)return 1;
 {FHDDuty duty;
  if(FHD_GetDuty(read32(ally,4u),&duty) &&
     (duty.kind==FHD_DUTY_ESCORT||duty.kind==FHD_DUTY_CLIENT) &&
     duty.peer==read32(v,4u) && duty.peer_seed==read32(v,0xe0u))return 1;
  if(FHD_GetDuty(read32(v,4u),&duty) &&
     (duty.kind==FHD_DUTY_ESCORT||duty.kind==FHD_DUTY_CLIENT) &&
     duty.peer==read32(ally,4u) && duty.peer_seed==read32(ally,0xe0u))return 1;
 }
 if(FHD_RelationPeer(read32(ally,4u),read32(v,4u),read32(v,0xe0u))>=15)return 1;
 if(ally[0x10u]==3u && v[0x10u]==3u && ally[0x514u]!=0u && ally[0x514u]==v[0x514u])return 1;
 return 0;
}
static int has_system_witness(const uint8_t *v,const uint8_t *a){
 const uint8_t *star,*list;const uint32_t *ships;uint32_t i,n;
 if(!v)return 0;
 star=(const uint8_t*)(uintptr_t)read32(v,0x24u);if(!star)return 0;
 list=(const uint8_t*)(uintptr_t)read32(star,0x2cu);if(!list)return 0;
 n=read32(list,8u);if(!n||n>MAX_SCAN_SHIPS)return 0;
 ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)return 0;
 for(i=0;i<n;i++){
   const uint8_t *other=(const uint8_t*)(uintptr_t)ships[i];
   if(!other||other==v||other==a)continue;
   if(!ordinary(other)&&!station_ship(other))continue;
   if(bonded_witness(other,v)||saw_ship(other,v))return 1;
 }
 return 0;
}
/* Determines whether the victim's HomePlanet ([v+0x2c]) receives a report of the
   attack/kill:
   1. Outlaws (pirates/smugglers) are never mourned by Coalition planets (Owner <= 4).
   2. State vessels (Diplomats, Liners, Warriors) have continuous telemetry.
   3. If HomePlanet is in the SAME star system, orbital sensors observe the incident.
   4. Otherwise, a private vessel's HomePlanet only learns of the attack/kill if there
      was at least one witness in radar/bonded range OR the battle lasted > 1 turn
      (giving the victim time to broadcast an SOS). A 1-turn silent ambush in deep space
      leaves no trace ("Тихое устранение без свидетелей"). */
static int incident_reported_to_home(const uint8_t *p,const uint8_t *v){
 const uint8_t *home;
 if(!p||!v)return 0;
 home=(const uint8_t*)(uintptr_t)read32(v,0x2cu);
 if(is_outlaw_victim(v)){
   if(!home||home[0x74u]<=4u)return 0;
 }
 if(is_state_vessel(v))return 1;
 if(home && read32(home,0x18u)!=0u && read32(home,0x18u)==read32(v,0x24u))return 1;
 if(has_system_witness(v,p))return 1;
 if(grudge_started_before_today(v,p,world_day()))return 1;
 return 0;
}
static void apply_role_planet_consequences(const uint8_t *k,const uint8_t *v,int is_kill){
 uint8_t *home;const uint8_t *star,*plist;const uint32_t *planets;uint32_t i,n;
 if(!g_armed||!k||!v||!player(k)||!loot_actor(v)||!same_star(k,v)||!fhd_planet_relation)return;
 if(!incident_reported_to_home(k,v))return;
 home=(uint8_t*)(uintptr_t)read32(v,0x2cu);
 if(home && home[0x75u]!=0u && (!is_outlaw_victim(v)||home[0x74u]==7u)){
   const uint8_t *hstar=(const uint8_t*)(uintptr_t)read32(home,0x18u);
   if(!hstar || !read32(hstar,0x44u)){
     if(is_kill){
       /* Victim's own HomePlanet becomes immediately Hostile (0) on reported kill. */
       FHDNPC_InvokePlanetRelation(home,(void*)k,-100);
     }else if(is_state_vessel(v)){
       /* Attacking an official Diplomat, Liner, or Warship carries an extra HomePlanet penalty. */
       FHDNPC_InvokePlanetRelation(home,(void*)k,-15);
     }
   }
 }
 /* Local jurisdiction planets in the CURRENT star system react when a peaceful
    Coalition ship (Diplomat, Liner, Transport) or Warship is destroyed there.
    Neighboring star systems remain unaffected. */
 if(!is_kill||is_outlaw_victim(v))return;
 if(!(v[0x10u]==2u||v[0x10u]==4u))return;
 star=(const uint8_t*)(uintptr_t)read32(v,0x24u);if(!star||read32(star,0x44u))return;
 plist=(const uint8_t*)(uintptr_t)read32(star,0x24u);if(!plist)return;
 n=read32(plist,8u);if(!n||n>32u)return;
 planets=(const uint32_t*)(uintptr_t)read32(plist,4u);if(!planets)return;
 for(i=0;i<n;i++){
   uint8_t *planet=(uint8_t*)(uintptr_t)planets[i];
   if(!planet||planet==home||!planet[0x75u]||planet[0x74u]>4u)continue;
   if(v[0x10u]==2u && (v[0x510u]==1u||v[0x510u]==2u)){
     /* Murdering a Diplomat or Passenger Liner strongly angers local Coalition planets. */
     FHDNPC_InvokePlanetRelation(planet,(void*)k,-30);
   }else if(v[0x10u]==2u){
     /* Destroying a peaceful Transport/Trader angers local Coalition planets in that system. */
     FHDNPC_InvokePlanetRelation(planet,(void*)k,-15);
   }else if(v[0x10u]==4u && planet[0x74u]==v[0x11u]){
     /* Destroying a garrison TWarrior angers same-race planets in that star system only. */
     FHDNPC_InvokePlanetRelation(planet,(void*)k,-35);
   }
 }
}
static void notify_bonded_witnesses(const uint8_t *v,const uint8_t *a,uint32_t weight,int is_kill){
 const uint8_t *star,*list;const uint32_t *ships;uint32_t i,n,aid,as,day;
 int outlaw;
 if(!g_armed||!ordinary(v)||!a||(!player(a)&&!ordinary(a))||!same_star(v,a))return;
 star=(const uint8_t*)(uintptr_t)read32(v,0x24u);if(!star)return;
 list=(const uint8_t*)(uintptr_t)read32(star,0x2cu);if(!list)return;
 n=read32(list,8u);if(!n||n>MAX_SCAN_SHIPS)return;
 ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)return;
 aid=read32(a,4u);as=read32(a,0xe0u);day=world_day();
 outlaw=is_outlaw_victim(v);
 for(i=0;i<n;i++){
   const uint8_t *ally=(const uint8_t*)(uintptr_t)ships[i];
   if(!ally||ally==a||ally==v||!ordinary(ally))continue;
   if(read32(ally,0x1cu)||read32(ally,0x20u)||ally[0x459u])continue;
   if(read32(ally,0x3f8u)==(uint32_t)(uintptr_t)a||
      read32(a,0x3f8u)==(uint32_t)(uintptr_t)ally)continue;
   if(bonded_witness(ally,v)){
     if(!ensure_captain(ally))continue;
     (void)FHD_RecordPeer(read32(ally,4u),aid,as,FHD_EVENT_ATTACK,weight,day);
     if(is_kill)track_grudge(ally,a);
     continue;
   }
   /* Ships outside radar range never react to a stranger's fight. */
   if(outlaw||!saw_ship(ally,v))continue;
   /* Direct radar witnesses react according to their role and personality: */
   if(ally[0x10u]==4u && ally[0x11u]<=4u){
     /* Coalition Warships in radar range condemn attacks on peaceful ships and intercept
        when a state vessel (Diplomat, Liner, or fellow Warship) is attacked. */
     if(ensure_captain(ally)){
       (void)FHD_RecordPeer(read32(ally,4u),aid,as,FHD_EVENT_ATTACK,is_kill?2u:1u,day);
       if(is_state_vessel(v) && ally[0x12du]>0u && !read32(ally,0x3f4u) &&
          !ally[0x43cu] && !ally[0x44cu]){
         FHDNPC_InvokeJoint((void*)ally,(void*)ally,(void*)a);
       }
     }
   }else if(is_kill && ally[0x10u]==1u && ally[0x11u]<=4u && !is_outlaw_victim(ally)){
     /* Honorable Warrior/Hero Rangers who directly witness a murder on radar condemn the killer. */
     FHDProfile prof;
     if(ensure_captain(ally) && FHD_ProfileOfIdentity(read32(ally,4u),read32(ally,0xe0u),&prof)==1 &&
        (prof.character==FHD_WARRIOR||prof.character==FHD_HERO) && prof.traits[4]>=65u){
       (void)FHD_RecordPeer(read32(ally,4u),aid,as,FHD_EVENT_ATTACK,1u,day);
     }
   }else if(is_kill && ally[0x10u]==2u && ally[0x11u]<=4u){
     /* Civilian transports witnessing a murder on radar become fearful of the killer. */
     if(ensure_captain(ally)){
       (void)FHD_RecordPeer(read32(ally,4u),aid,as,FHD_EVENT_EXTORTION,1u,day);
     }
   }
 }
}
void FHDNPC_GrievanceOnAttack(void*victim,void*attacker){
 const uint8_t*v=victim,*a=attacker;
 if(!g_armed||!ordinary(v)||!a||!same_star(v,a))return;
 if(player(a)){
   first_contact(v,a,2u);g_stat[1]++;
   FHDNPC_LocalDialogue(FHD_DLG_ATTACK,v,read32(a,4u),1u,world_day(),0u);
 }
 if(player(a)||ordinary(a))track_grudge(v,a);
 if((player(a)||ordinary(a))&&!verified_defense(a,v)){
   if(player(a))apply_role_planet_consequences(a,v,0);
   notify_bonded_witnesses(v,a,1u,0);
 }
}
uint32_t FHDNPC_ShouldSuppressReputation(void*ship,void*victim){
 uint32_t result=verified_defense(ship,victim);
 if(result)g_stat[2]++;else g_stat[3]++;
 /* Suppress the vanilla galaxy-wide racial/planetary penalty loop (0x71796a..0x717c84)
    for all ordinary ships so unrelated planets and random same-race pilots across
    the current and neighboring systems do not artificially lose attitude.
    The victim's own HomePlanet branch at 0x317c89 remains controlled by
    FHDNPC_IsDefenseVerified (skipped on self-defense, outlaw targets, or silent kills). */
 if(g_armed&&player((const uint8_t*)ship)&&ordinary((const uint8_t*)victim)&&
    same_star((const uint8_t*)ship,(const uint8_t*)victim))return 1u;
 return result;
}
/* An unscaled native HOME PLANET relation branch appears at RVA 0x317C89.
   Zeroing the severity does NOT suppress it, so skip ONLY that branch for
   verified self-defense, outlaw targets (pirates/smugglers), or silent kills
   without witnesses. Never bypass original achievements. */
uint32_t FHDNPC_IsDefenseVerified(void*ship,void*victim){
 const uint8_t *p=(const uint8_t*)ship,*v=(const uint8_t*)victim;
 uint32_t result=verified_defense(p,v);
 if(result){g_stat[14]++;return 1u;}
 if(g_armed&&player(p)&&ordinary(v)&&same_star(p,v)){
   uint32_t vid=read32(v,4u),vseed=read32(v,0xe0u),pseed=read32(p,0xe0u);
   if(g_last_kill_defense.valid &&
      g_last_kill_defense.vid==vid && g_last_kill_defense.vseed==vseed &&
      g_last_kill_defense.pseed==pseed && g_last_kill_defense.star==star_id(v) &&
      g_last_kill_defense.day==world_day()){
     if(g_last_kill_defense.suppress_home){g_stat[14]++;return 1u;}
     return 0u;
   }
   if(!incident_reported_to_home(p,v)){g_stat[14]++;return 1u;}
 }
 return 0u;
}
/* Delphi TObjectList/TList on Win32: +4 pointer array, +8 count. */
static int list_has(const uint8_t*list,const void*element){
 uint32_t count,i;const uint32_t *array;
 if(!list||!element)return 0;
 count=read32(list,8u);
 if(!count||count>MAX_SCAN_SHIPS)return 0;
 array=(const uint32_t*)(uintptr_t)read32(list,4u);if(!array)return 0;
 for(i=0;i<count;i++)if(array[i]==(uint32_t)(uintptr_t)element)return 1;
 return 0;
}
static int collecting(const uint8_t *ship,const uint8_t *item){
 const uint8_t *targets=(const uint8_t *)(uintptr_t)read32(ship,0x3d8u);
 if(list_has(targets,item))return 1;
 if(ship[0x43cu])return 0;
 if(!ship[0x44cu] && ship[0x428u]==1u){
   /* The stock GetCurrentPickupItem also recognizes the computed approach
      destination. Check this exact item without searching the whole system. */
   typedef void (__attribute__((regparm(3))) *Approach)(const uint8_t *,const void *,void *);
   uint32_t point[2];
   ((Approach)(g_base+APPROACH_RVA))(ship,item+0x10u,point);
   if(point[0]==read32(ship,0x434u)&&point[1]==read32(ship,0x438u))return 1;
 }
 /* In the player's star system, the 9-substep tractor beam animation at
    0x7FC7BA drags item.x/item.y toward the picking ship BEFORE calling
    CompleteItemPickup (0x7F4F5C), shifting ComputeApproachPoint away from
    the witness's initial OrderPos (0x434/0x438). Check verified TShip.FreeSpace
    (+0xc4 in TryCollectItems 0x6e1889) with fallback to +0xdc, and allow
    mod-scripted roles (SpecAgents, Freelancers, Explorers) with a working hook
    in immediate proximity to register the interference. */
 {int32_t free_space=(int32_t)read32(ship,0xc4u);
  if(free_space<=0)free_space=(int32_t)read32(ship,0xdcu);
  if(read32(ship,0x110u)!=0u && free_space>0 &&
     free_space>=(int32_t)read32(item,0x1cu) &&
     !ship[0x3b4u] && !read32(ship,0x3f4u) && ship[0x428u]<=1u){
    union {float f;uint32_t u;} ix,iy,sx,sy,ox,oy;
    ix.u=read32(item,0x10u);iy.u=read32(item,0x14u);
    if(ship[0x428u]==1u){
      ox.u=read32(ship,0x434u);oy.u=read32(ship,0x438u);
      float dx=ox.f-ix.f,dy=oy.f-iy.f;
      if(dx*dx+dy*dy<=550.0f*550.0f)return 1;
    }
    sx.u=read32(ship,0x14u);sy.u=read32(ship,0x18u);
    {float dx=sx.f-ix.f,dy=sy.f-iy.f;
     if(dx*dx+dy*dy<=420.0f*420.0f)return 1;
    }
  }
 }
 return 0;
}
uint32_t FHDNPC_RelationLevelValue(const uint8_t *observer,const uint8_t *peer,uint32_t native_value){
 native_value&=0x7fu;
 if(!g_armed||native_value<10u||native_value>100u||observer==peer||
    !loot_actor(observer)||(!loot_actor(peer)&&!player(peer)))return native_value;
 const uint8_t *galaxy=(const uint8_t *)(uintptr_t)read32((const void *)g_base,GALAXY_RVA);
 if(!galaxy||galaxy[0x1d8u])return native_value;
 int32_t memory=FHD_RelationPeer(read32(observer,4u),read32(peer,4u),read32(peer,0xe0u));
 int32_t value=(int32_t)native_value+memory;
 /* Memory can sour a relationship. The engine's existing hostility and
    scripted/diplomatic checks retain control of the hostile band. */
 return value<0?0u:value>100?100u:(uint32_t)value;
}
static void capture(uint8_t*ship,uint8_t*item,uint8_t*star,FHDLootFrame*f,uint32_t kind){
 uint32_t i,n;const uint32_t*ships;uint8_t*list,*other;
 if(!f)return;
 {uint32_t*x=(uint32_t*)f;for(i=0;i<sizeof(*f)/4u;i++)x[i]=0;}
 if(!g_armed||(!player(ship)&&!ordinary(ship))||!item||!star||read32(ship,0x24u)!=(uint32_t)(uintptr_t)star)return;
 if((kind!=3u&&(int32_t)read32(item,0x30u)>0)||!read32(item,8u)||!list_has((uint8_t*)(uintptr_t)read32(star,0x30u),item))return;
 list=(uint8_t*)(uintptr_t)read32(star,0x2cu);if(!list)return;
 n=read32(list,8u);if(!n||n>MAX_SCAN_SHIPS)return;
 ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)return;
 f->kind=kind;f->item=(uint32_t)(uintptr_t)item;f->item_id=read32(item,8u);
 f->actor_id=read32(ship,4u);f->actor_seed=read32(ship,0xe0u);
 f->star=(uint32_t)(uintptr_t)star;f->cost=read32(item,0x20u);f->day=world_day();
 for(i=0;i<n&&f->count<FHD_LOOT_WITNESSES;i++){
   other=(uint8_t*)(uintptr_t)ships[i];
   if(other==ship||!loot_actor(other)||read32(other,0x24u)!=f->star||
      read32(other,0x1cu)||read32(other,0x20u)||other[0x459u])continue;
   if(collecting(other,item)&&saw_item(other,item)){
     FHDLootWitness*w=&f->witness[f->count++];
     w->id=read32(other,4u);w->seed=read32(other,0xe0u);w->kind=other[0x10u];
   }
 }
 f->valid=1u;
}
void FHDNPC_CaptureLootPickup(void*parent,FHDLootFrame*out){
 uint8_t*frame=parent;
 if(!out)return;
 if(!frame){out->valid=0;return;}
 capture((uint8_t*)(uintptr_t)read32(frame-8u,0u),
         (uint8_t*)(uintptr_t)read32(frame-4u,0u),
         (uint8_t*)(uintptr_t)read32(frame-12u,0u),out,1u);
}
void FHDNPC_CaptureLootShot(void*ship,void*item,FHDLootFrame*out){
 uint8_t*p=ship;
 if(!out)return;
 capture(p,item,p?(uint8_t*)(uintptr_t)read32(p,0x24u):0,out,2u);
}
static int already_recorded(uint32_t id,uint32_t seed,uint32_t item,uint32_t day,uint32_t kind){
 uint32_t i=(id*2654435761u^seed^item^kind*0x9e3779b9u)&(CONFLICT_SLOTS-1u);
 if(g_loot_seen[i].id==id&&g_loot_seen[i].seed==seed&&
    g_loot_seen[i].item==item&&g_loot_seen[i].day==day&&g_loot_seen[i].kind==kind)return 1;
 g_loot_seen[i].id=id;g_loot_seen[i].seed=seed;
 g_loot_seen[i].item=item;g_loot_seen[i].day=day;g_loot_seen[i].kind=kind;
 return 0;
}
static void complete_loot(FHDLootFrame*f,int confirmed_impact){
 uint32_t i,n;const uint32_t*ships;uint8_t*star,*actor,*other,*list;FHDProfile prof;
 if(!g_armed||!f||!f->valid||!f->count)return;
 star=(uint8_t*)(uintptr_t)f->star;
 /* The original routine must actually have removed this exact item from
    the star; shots that merely damaged it and rejected scripted pickups
    never turn into a verified theft or destruction. */
 if(!confirmed_impact&&list_has((uint8_t*)(uintptr_t)read32(star,0x30u),(void*)(uintptr_t)f->item))return;
 list=(uint8_t*)(uintptr_t)read32(star,0x2cu);
 if(!list)return;
 n=read32(list,8u);if(!n||n>MAX_SCAN_SHIPS)return;
 ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)return;
 actor=0;for(i=0;i<n;i++){
   uint8_t*p=(uint8_t*)(uintptr_t)ships[i];
   if((player(p)||ordinary(p))&&read32(p,4u)==f->actor_id&&read32(p,0xe0u)==f->actor_seed){actor=p;break;}
 }
 if(!actor)return;
 int is_player=player(actor);
 for(i=0;i<f->count;i++){
   uint32_t j,event,magnitude;int32_t reaction;int32_t next_memory=1000;
   FHDLootWitness*w=&f->witness[i];other=0;
   for(j=0;j<n;j++){
     uint8_t*p=(uint8_t*)(uintptr_t)ships[j];
     if(loot_actor(p)&&read32(p,4u)==w->id&&read32(p,0xe0u)==w->seed){other=p;break;}
   }
   if(!other||read32(other,0x24u)!=f->star||!ensure_captain(other)||
      FHD_ProfileOfIdentity(w->id,w->seed,&prof)!=1||
      already_recorded(w->id,w->seed,f->item_id,f->day,f->kind))continue;
   event=f->kind==1u?FHD_EVENT_LOOT_TAKEN:FHD_EVENT_LOOT_DESTROYED;
   magnitude=1u; /* Exactly one attitude grade for every verified conflict. */
   if(f->kind==1u)magnitude=1u; /* pickup is equally weighted with destruction */
   int32_t prior=FHD_RelationPeer(w->id,f->actor_id,f->actor_seed);
   if(fhd_relation_value){
      /* This native method is already used by the existing relation-level hook.
         Delphi register ABI: EAX=observer, EDX=peer, AL=0..100 value. */
      typedef uint8_t (__attribute__((regparm(2))) *RelationToShip)(const void*,const void*);
      uint32_t native=((RelationToShip)fhd_relation_value)(other,actor);
      /* Civilians and Coalition military (vs Coalition ships) may lower attitude
         or demand restitution, but never become hostile (<10) solely over loot. */
      uint32_t civilian=(prof.role==FHD_ROLE_TRADER||prof.role==FHD_ROLE_LINER||
          prof.role==FHD_ROLE_DIPLOMAT||prof.role==FHD_ROLE_TRANSPORT||
          (w->kind==4u && other[0x11u]<=4u && actor[0x11u]<=4u));
      next_memory=FHDNPC_LootNextGradeMemoryMin(native,prior,civilian?10u:0u);
   }
   if(FHD_RecordPeer(w->id,f->actor_id,f->actor_seed,event,magnitude,f->day)!=1)continue;
   if(next_memory!=1000)(void)FHD_AdjustRecordedPeer(w->id,f->actor_id,f->actor_seed,next_memory,f->day);
   g_stat[f->kind==1u?4:5]++;
   /* NPC disputes affect memories without invoking a delivery or issuing demands. */
   g_stat[(is_player?15u:17u)+(f->kind!=1u)]++;
   if(confirmed_impact)g_stat[19]++;
   if(!is_player)continue; /* NPC-on-NPC loot stays out of player radio. */
   reaction=FHDNPC_LootReaction(&prof,f->kind==2u,1u,
       numeric_strength(actor)>numeric_strength(other));
   unsigned line=f->kind==1u?FHD_DLG_LOOT_TAKEN:FHD_DLG_LOOT_DESTROYED;
   if(w->kind==3u){
     if(f->kind==2u&&prior<=-20&&reaction>=2&&numeric_strength(other)>=numeric_strength(actor))
       line=FHD_DLG_THREAT;
     else if(reaction>=2)line=f->kind==1u?FHD_DLG_PIRATE_TAKE:FHD_DLG_PIRATE_DESTROY;
   }else if(reaction>=2&&f->kind==2u)line=FHD_DLG_DEMAND;
   if(reaction||w->kind==3u||(f->kind==2u&&magnitude>=3u))
     FHDNPC_LocalDialogue(line,other,f->actor_id,1u,f->day,f->item_id);
   if(reaction==1)g_stat[6]++;
   if(reaction==3)g_stat[8]++;
   /* Demand restitution selectively based on pilot personality, current attitude,
      and whether this is a repeat offense. */
   {uint32_t base_rel=50u;
    if(fhd_relation_value){
      typedef uint8_t (__attribute__((regparm(2))) *RelationToShip)(const void*,const void*);
      base_rel=((RelationToShip)fhd_relation_value)(other,actor);
    }
    int32_t eff_before=(int32_t)base_rel+prior;
    if(eff_before<0)eff_before=0;if(eff_before>100)eff_before=100;
    int stronger=numeric_strength(actor)>numeric_strength(other);
    if(should_demand_compensation(&prof,f->kind,f->cost,eff_before,prior,stronger))
      {g_pending_pre_rel=(uint32_t)eff_before;queue_claim(other,actor,f->item_id,f->day,f->kind,f->cost);}
   }
 }
}
void FHDNPC_CompleteLoot(FHDLootFrame *frame){complete_loot(frame,0);}
void FHDNPC_MissileLoot(uint8_t *star,uint8_t *item,const uint8_t *missile){
 if(!missile||!item||read32(item,0x2cu))return;
 /* These two call sites execute after the engine has accepted the hit and
    committed to removing the original item, before clearing pickup targets. */
 uint8_t *actor=(uint8_t *)(uintptr_t)read32(missile,0x3cu);
 FHDLootFrame frame;
 capture(actor,item,star,&frame,3u);
 frame.kind=2u;
 complete_loot(&frame,1);
}
/* Do not call game native function pointers until all actor/target checks
   have passed and no script-managed ship or protected class is involved. */
static void *find_star_ship(const uint8_t*star,uint32_t id,uint32_t seed){
 uint32_t i,count;const uint32_t*ships;const uint8_t*list;
 if(!star)return 0;
 list=(const uint8_t*)(uintptr_t)read32(star,0x2cu);if(!list)return 0;
 count=read32(list,8u);if(!count||count>MAX_SCAN_SHIPS)return 0;
 ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)return 0;
 for(i=0;i<count;i++){
   uint8_t*p=(uint8_t*)(uintptr_t)ships[i];
   if((ordinary(p)||player(p))&&read32(p,4u)==id&&read32(p,0xe0u)==seed)return p;
 }
 return 0;
}
/* A bounty is an actual conditional reward payable only for the verified
   ProcessShipKill callback. No money is removed merely for announcing it. */
API uint32_t FHDNPC_BountyFor(uint32_t offender_id){
 uint32_t i,sum=0,day=world_day();
 for(i=0;i<OFFER_SLOTS;i++)if(g_offers[i].issued&&!g_offers[i].paid&&
     day>=g_offers[i].day&&day-g_offers[i].day<=60u&&g_offers[i].target==offender_id)sum+=g_offers[i].amount;
 return sum;
}
static int offer_exists(uint32_t issuer,uint32_t iseed,uint32_t target,
                        uint32_t tseed,uint32_t day){
 for(uint32_t i=0;i<OFFER_SLOTS;i++){
   if(g_offers[i].issued && g_offers[i].issuer==issuer &&
      g_offers[i].iseed==iseed && g_offers[i].target==target && g_offers[i].tseed==tseed &&
      day>=g_offers[i].day && day-g_offers[i].day<=60u)return 1;
 }
 return 0;
}
static void process_claim(uint8_t *issuer,uint8_t *star,uint32_t day){
 uint32_t i=hash_claim(read32(issuer,4u),read32(issuer,0xe0u));
 if(!g_claims[i].active||g_claims[i].issuer!=read32(issuer,4u)||
    g_claims[i].seed!=read32(issuer,0xe0u))return;
 if(day<g_claims[i].day||day-g_claims[i].day>10u ||
    g_claims[i].star!=star_id(issuer)){g_claims[i].active=0;return;}
 /* Do not make conversations with other NPCs or across star systems. */
 uint8_t *target=find_star_ship(star,g_claims[i].player_id,g_claims[i].player_seed);
 if(!player(target)||!same_star(issuer,target)){g_claim_block[0]++;return;}
 /* Native TryExtortShip is invoked from the ordinary ship AI tick.
    A current enemy equal to the PLAYER is legitimate: vanilla pirates
    themselves demand money while pursuing their enemy. Preserve a true
    other enemy and script-driven absolute orders. */
 if((!g_claims[i].amount && issuer[0x44cu])||
    (read32(issuer,0x3f4u) && read32(issuer,0x3f4u)!=(uint32_t)(uintptr_t)target)){
    g_claim_block[1]++;return;
 }
 /* The original TShip.TryExtortShip has its own contact, cargo, current
    player, and 7-turn guards. Direct restitution claims bypass the unrelated
    7-turn galaxy piracy cooldown while still respecting DIALOGUE_COUNT_RVA. */
 if((!g_claims[i].amount && read32((void*)g_base,EXTORT_COOLDOWN_RVA)<7u)||
    *((const uint8_t*)(g_base+DIALOGUE_COUNT_RVA))>=1u){
    g_claim_block[2]++;return;
 }
 if(*((const uint8_t*)(g_base+AUTO_PLAYER_RVA))||
    issuer[0x49au]||target[0x49au]||
    read32(target,0x1cu)||read32(target,0x20u)||target[0x459u]){
    g_claim_block[4]++;return;
 }
 /* Do not consume the claim before a radar/contact window exists.
    If the claimant is temporarily out of radio range in open space, steer
    toward the player so the demand is delivered on the next turn. */
 typedef uint8_t (__attribute__((regparm(2))) *NativeCanContact)(const void*,const void*);
 if(!((NativeCanContact)(g_base+CAN_CONTACT_RVA))(issuer,target)){
    if(!read32(issuer,0x1cu)&&!read32(issuer,0x20u)&&issuer[0x428u]<=1u&&!issuer[0x43cu]&&!issuer[0x44cu]){
      typedef void (__attribute__((regparm(3))) *OrderMove)(void*,const void*,unsigned);
      ((OrderMove)(g_base+ORDER_MOVE_RVA))(issuer,target+0x14u,0u);
    }
    g_claim_block[3]++;return;
 }
 /* Replaces FHDNPC_InvokeExtort with TRanger.BuildMoneyExtortionResponse
    (FHDNPC_InvokeMoneyDemand) so the exact 2x item.Cost amount is demanded
    via ShowPlayerDialogue, which handles player choice, payment transfer,
    and truce without directly writing to Money fields. */
 if(g_claims[i].last_attempt==day)return;
 if(!g_claims[i].amount)return;
 g_claims[i].last_attempt=day;
 uint32_t before=*((const uint8_t*)(g_base+DIALOGUE_COUNT_RVA));
 uint16_t *native_response=0;
 g_claim_block[5]++;
 *((uint8_t*)(g_base+DIALOGUE_COUNT_RVA))=(uint8_t)(before+1u);
 *((uint32_t*)(g_base+EXTORT_COOLDOWN_RVA))=0u;
 uint32_t ok=FHDNPC_InvokeMoneyDemand((void*)(g_base+MONEY_RESPONSE_RVA),
      target,issuer,&native_response,g_claims[i].amount);
 if(native_response)SysFreeString(native_response);
 g_stat[9]++;g_claim_results[0]++;
 if(ok)g_claim_results[1]++;
 else if(g_claims[i].pre_rel>=30u && fhd_relation_value){
   /* Native extortion refusal can subtract extra native relation on top of our
      loot grade drop. Keep a formerly Neutral/Good pilot in the Bad band (18)
      instead of skipping straight from Neutral to Hostile (<10) on the first incident. */
   typedef uint8_t (__attribute__((regparm(2))) *RelationToShip)(const void*,const void*);
   uint32_t native_after=((RelationToShip)fhd_relation_value)(issuer,target);
   int32_t cur_mem=FHD_RelationPeer(g_claims[i].issuer,g_claims[i].player_id,g_claims[i].player_seed);
   if(native_after<=100u && (int32_t)native_after+cur_mem<10){
     (void)FHD_AdjustRecordedPeer(g_claims[i].issuer,g_claims[i].player_id,g_claims[i].player_seed,
                                  18-(int32_t)native_after,day);
   }
 }
 /* The native player branch always invokes ShowPlayerDialogue, even when
    the player refuses payment (ok=0). A refusal is a finished conversation. */
 g_claim_results[2]++;
 uint32_t after=*((const uint8_t*)(g_base+DIALOGUE_COUNT_RVA));
 if(ok||after>before)g_claims[i].active=0;
 else g_claim_results[3]++;
 g_claims[i].active=0;
}
void FHDNPC_GrievanceTick(void*ship,uint32_t kind){
 uint8_t*p=ship,*other,*merc,*star;uint32_t i,id,seed,day,budget,amount,plan,idx=OFFER_SLOTS,mid,ms;
 FHDProfile prof,mp;const uint32_t*ships;uint32_t n,j;
 if(g_armed&&kind<=3u&&loot_actor(p)&&!ordinary(p)){
   star=(uint8_t*)(uintptr_t)read32(p,0x24u);
   if(star)process_claim(p,star,world_day());
   return;
 }
 if(!g_armed||kind>3u||!ordinary(p))return;
 id=read32(p,4u);seed=read32(p,0xe0u);day=world_day();
 star=(uint8_t*)(uintptr_t)read32(p,0x24u);if(!star)return;
 process_claim(p,star,day); /* complaint can open even without bounty funds */
 /* A pirate's or armed combatant's confirmed interference that pushes
    effective relation into the hostile band (< 10) becomes open hostility.
    Ordinary civilian pilots never initiate combat for mineral/property disputes. */
 FHDProfile attacker_profile;
 int has_prof=(FHD_ProfileOfIdentity(id,seed,&attacker_profile)==1);
 int armed_resentment=(kind==2u || kind==0u ||
    (kind==1u && has_prof &&
      (attacker_profile.role==FHD_ROLE_SMUGGLER||attacker_profile.role==FHD_ROLE_MERCENARY))) &&
    (!has_prof || attacker_profile.traits[1]>=65u);
 if(armed_resentment && !read32(p,0x3f4u) &&
    !read32(p,0x3f8u) && !p[0x43cu] && !p[0x44cu] && !p[0x3b4u] &&
    !p[0x459u] && !read32(p,0x1cu) && !read32(p,0x20u)){
   uint32_t player_id=read32((void*)g_base,ENCODED_PLAYER_RVA)^PLAYER_XOR_KEY;
   uint8_t *target=(uint8_t*)(uintptr_t)player_id;
   if(target && player(target) && same_star(p,target) &&
      !read32(target,0x1cu) && !read32(target,0x20u) &&
      !target[0x459u] && !read32(target,0x3f8u)){
     int32_t grudge=FHD_RelationPeer(id,read32(target,4u),read32(target,0xe0u));
     if(grudge<0 && p[0x12du]>0u && fhd_relation_value){
       typedef uint8_t (__attribute__((regparm(2))) *RelationToShip)(const void*,const void*);
       uint32_t native=((RelationToShip)fhd_relation_value)(p,target);
       if(native<=100u && (int32_t)native+grudge<10 &&
          numeric_strength(p)>=numeric_strength(target)){
         FHDNPC_InvokeJoint(p,p,target); /* engine validates actual target */
         if(read32(p,0x3f4u)==(uint32_t)(uintptr_t)target)g_pirate_retaliations++;
       }
     }
   }
 }

 if(read32(p,0x3f4u)||p[0x44cu]||p[0x43cu]||p[0x428u]==6u)return;
 if(FHD_ProfileOf(id,&prof)!=1)return;
 budget=read32(p,0xf0u);if(budget<2000u)return;
 for(i=(id*16777619u^seed)&(CONFLICT_SLOTS-1u);
     i==((id*16777619u^seed)&(CONFLICT_SLOTS-1u));i++) {
   uint32_t target;
   if(g_grudges[i].issuer!=id||g_grudges[i].iseed!=seed||
      g_grudges[i].star!=star_id(p)||
      day<g_grudges[i].day||day-g_grudges[i].day>120u)continue;
   target=g_grudges[i].target;
   other=find_star_ship(star,target,g_grudges[i].tseed);
   if(!other||other==p||read32(p,0x3f8u)==(uint32_t)(uintptr_t)other||
      read32(other,0x3f8u)==(uint32_t)(uintptr_t)p||
      !FHD_RelationPeer(id,target,g_grudges[i].tseed)||offer_exists(id,seed,target,g_grudges[i].tseed,day))continue;
   plan=(uint32_t)FHDNPC_GrudgePlan(&prof,FHD_RelationPeer(id,target,g_grudges[i].tseed),
        budget,numeric_strength(other));
   if(plan<2u)continue;
   for(j=0;j<OFFER_SLOTS;j++)if(!g_offers[j].issued||g_offers[j].paid||
      (day>=g_offers[j].day&&day-g_offers[j].day>60u)){idx=j;break;}
   if(idx==OFFER_SLOTS)break;
   amount=budget/5u;if(amount>50000u)amount=50000u;
   if(amount<750u)continue;
   merc=0;
   {const uint8_t*list=(const uint8_t*)(uintptr_t)read32(star,0x2cu);
    if(!list)continue;n=read32(list,8u);if(n>MAX_SCAN_SHIPS)continue;
    ships=(const uint32_t*)(uintptr_t)read32(list,4u);if(!ships)continue;}
   for(j=0;j<n;j++){
      uint8_t*q=(uint8_t*)(uintptr_t)ships[j];
      if(q==p||q==other||!ordinary(q)||read32(q,0x3f4u)||
         q[0x44cu]||q[0x43cu]||read32(q,0x3f8u)==(uint32_t)(uintptr_t)other||
         read32(q,0x3fcu)||read32(q,0x430u)||q[0x458u])continue;
      mid=read32(q,4u);ms=read32(q,0xe0u);
      if(FHD_ProfileOf(mid,&mp)!=1)continue;
      if(!(mp.character==FHD_MERCENARY||mp.role==FHD_ROLE_MERCENARY||
           FHDNPC_LookupVerifiedProfession(mid,ms,q[0x10u])==13u))continue;
      if(FHD_RelationPeer(mid,target,g_grudges[i].tseed)>20)continue; /* no hiring a friend of the target */
      merc=q;break;
   }
   if(!merc&&plan==2u)continue; /* a paid contract needs a real executor */
   g_offers[idx].issuer=id;g_offers[idx].iseed=seed;
   g_offers[idx].target=target;g_offers[idx].tseed=g_grudges[i].tseed;
   g_offers[idx].executor=merc?read32(merc,4u):0u;
   g_offers[idx].eseed=merc?read32(merc,0xe0u):0u;
   g_offers[idx].amount=amount;g_offers[idx].day=day;
   g_offers[idx].issued=1u;g_offers[idx].paid=0u;
   g_offers[idx].star=star_id(p);
   g_stat[10]++;
   FHDNPC_LocalDialogue(FHD_DLG_CONTRACT,p,target,0u,day,amount);
   if(merc){
      /* Exact engine SetJointAttackTarget called with merc as BOTH allies:
         this changes merc's goals only, not the innocent issuer's order. */
      FHDNPC_InvokeJoint(merc,merc,other);g_stat[11]++;
   }
   break; /* at most one contract created per NPC tick */
 }
}
static void pay_bounty(void*killer,void*victim){
 uint8_t*k=killer,*v=victim,*star,*issuer;uint32_t i,kid,vid,ks,day;
 if(!g_armed||!(ordinary(k)||player(k))||!ordinary(v)||!same_star(k,v))return;
 kid=read32(k,4u);vid=read32(v,4u);ks=read32(k,0xe0u);
 star=(uint8_t*)(uintptr_t)read32(v,0x24u);day=world_day();
 for(i=0;i<OFFER_SLOTS;i++){
   uint32_t amount;
   if(!g_offers[i].issued||g_offers[i].paid||g_offers[i].target!=vid||
       g_offers[i].tseed!=read32(v,0xe0u)||g_offers[i].star!=star_id(v)||
       day<g_offers[i].day||day-g_offers[i].day>60u||
       (g_offers[i].executor && (g_offers[i].executor!=kid||g_offers[i].eseed!=ks)))continue;
   issuer=find_star_ship(star,g_offers[i].issuer,g_offers[i].iseed);
   if(!issuer||issuer==k||!ordinary(issuer))continue;
   amount=g_offers[i].amount;
   if(read32(issuer,0xf0u)<amount || read32(k,0xf0u)>100000000u-amount)continue;
   if(amount>0u && g_offers[i].paid==0u){
      g_offers[i].paid=1u; /* idempotent before money changes */
      FHDNPC_InvokeSetMoney(issuer,read32(issuer,0xf0u)-amount);
      FHDNPC_InvokeSetMoney(k,read32(k,0xf0u)+amount);
      g_stat[12]++;g_stat[13]+=amount;
   }
 }
}
void FHDNPC_OnNativeKill(void*killer,void*victim){
 const uint8_t *k=(const uint8_t*)killer,*v=(const uint8_t*)victim;
 pay_bounty(killer,victim);
 if(g_armed&&k&&v&&loot_actor(v)&&same_star(k,v)&&(player(k)||loot_actor(k))){
   uint32_t defense=verified_defense(k,v);
   if(player(k)){
     int reported=incident_reported_to_home(k,v);
     g_last_kill_defense.vid=read32(v,4u);
     g_last_kill_defense.vseed=read32(v,0xe0u);
     g_last_kill_defense.pseed=read32(k,0xe0u);
     g_last_kill_defense.star=star_id(v);
     g_last_kill_defense.day=world_day();
     g_last_kill_defense.defense=defense;
     g_last_kill_defense.suppress_home=(defense || !reported)?1u:0u;
     g_last_kill_defense.valid=1u;
     if(!defense && reported)apply_role_planet_consequences(k,v,1);
   }
   if(!defense)notify_bonded_witnesses(v,k,3u,1);
 }
 if(ordinary(victim))FHD_Forget(read32(victim,4u));
}
void FHDNPC_GrievanceExtraStats(uint32_t*out){if(out)out[0]=g_pirate_retaliations;}
void FHDNPC_CompensationDiagnostics(uint32_t*out){if(out)for(uint32_t i=0;i<6u;i++)out[i]=g_claim_block[i];}
void FHDNPC_ClaimResults(uint32_t*out){if(out)for(uint32_t i=0;i<4u;i++)out[i]=g_claim_results[i];}
void FHDNPC_GrievanceStats(uint32_t*out){uint32_t i;if(!out)return;
 for(i=0;i<20u;i++)out[i]=g_stat[i];}
void FHDNPC_ResetGrievance(void){uint32_t i;
 for(i=0;i<CONFLICT_SLOTS;i++){g_loot_seen[i].id=0;g_grudges[i].issuer=0;}
 for(i=0;i<OFFER_SLOTS;i++)g_offers[i].issued=0;
 for(i=0;i<CLAIM_SLOTS;i++)g_claims[i].active=0;
 for(i=0;i<20u;i++)g_stat[i]=0;
 g_last_kill_defense.valid=0u;
 g_pirate_retaliations=0u;
 for(i=0;i<6u;i++)g_claim_block[i]=0u;
 for(i=0;i<4u;i++)g_claim_results[i]=0u;
 {extern void FHDNPC_StationReset(void);FHDNPC_StationReset();}
}
static int same_bytes(const uint8_t*a,const uint8_t*b,uint32_t n){uint32_t i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int protect_write(void*target,const uint8_t*expected,const uint8_t*value,uint32_t n){
 uint32_t old=0,unused=0,i;uint8_t*p=target;
 if(!VirtualProtect(target,n,PROT,&old))return 0;
 if(!same_bytes(p,expected,n)){VirtualProtect(target,n,old,&unused);return 0;}
 for(i=0;i<n;i++)p[i]=value[i];
 FlushInstructionCache(GetCurrentProcess(),target,n);
 VirtualProtect(target,n,old,&unused);return 1;
}
static int install(uint32_t index,void*stub){
 uint8_t value[10];uint8_t*target=(uint8_t*)g_base+k_rva[index];uint32_t n=k_len[index],i;
 int32_t rel=(int32_t)((uintptr_t)stub-((uintptr_t)target+5u));
 value[0]=(index<REQUIRED_SITES||k_rva[index]==RELATION_VALUE_RVA||k_rva[index]==FORM_INFO_RVA)?0xe9u:0xe8u;
 for(i=0;i<4u;i++)value[i+1u]=(uint8_t)((uint32_t)rel>>(i*8u));
 for(i=5u;i<n;i++)value[i]=0x90u;
 return protect_write(target,k_sig[index],value,n);
}
static int uninstall(uint32_t index){
 uint8_t value[10];uint8_t*target=(uint8_t*)g_base+k_rva[index];uint32_t n=k_len[index],i;
 int32_t rel=(int32_t)((uintptr_t)k_stubs[index]-((uintptr_t)target+5u));
 value[0]=(index<REQUIRED_SITES||k_rva[index]==RELATION_VALUE_RVA||k_rva[index]==FORM_INFO_RVA)?0xe9u:0xe8u;
 for(i=0;i<4u;i++)value[i+1u]=(uint8_t)((uint32_t)rel>>(i*8u));
 for(i=5u;i<n;i++)value[i]=0x90u;
 return protect_write(target,value,k_sig[index],n);
}
static int vmt_set(void*expected,void*replacement){
 uint32_t old=0,unused=0;void**slot=(void**)(g_base+PLAYER_REACT_SLOT);
 if(!VirtualProtect(slot,4u,PROT,&old))return 0;
 if(*slot!=expected){VirtualProtect(slot,4u,old,&unused);return 0;}
 *(volatile void**)slot=replacement;
 VirtualProtect(slot,4u,old,&unused);return 1;
}
int32_t FHDNPC_ArmGrievance(void){
 uint32_t i,j;uint8_t*b=(uint8_t*)GetModuleHandleW(0);
 if(g_armed)return 1;
 if(!b)return 0;
 g_base=(uintptr_t)b;
 if(read32(b,PLAYER_REACT_SLOT)!=((uint32_t)g_base+RANGER_REACT_RVA))return 0;
 {const uint8_t radar_sig[]={0x53,0x56,0x8b,0xd8,0x8b},
                approach_sig[]={0x53,0x56,0x57,0x83,0xc4,0xf4};
  if(!same_bytes(b+0x2d7288u,radar_sig,sizeof(radar_sig))||
     !same_bytes(b+APPROACH_RVA,approach_sig,sizeof(approach_sig)))return 0;}
 for(i=0;i<REQUIRED_SITES;i++)if(!same_bytes(b+k_rva[i],k_sig[i],k_len[i]))return 0;
 fhd_player_react_orig=b+RANGER_REACT_RVA;
 fhd_reputation_continue=b+REPUTATION_RVA+k_len[0];
 fhd_pickup_continue=b+PICKUP_RVA+k_len[1];
 fhd_shot_continue=b+SHOT_RVA+k_len[2];
 fhd_kill_continue=b+KILL_RVA+k_len[3];
 fhd_home_continue=b+HOME_RVA+k_len[4];
 fhd_home_end=b+HOME_END_RVA;
 fhd_set_money=b+SET_MONEY_RVA;
 fhd_joint_attack=b+JOINT_RVA;
 fhd_planet_relation=b+PLANET_CHANGE_REL_RVA;
 for(i=0;i<REQUIRED_SITES;i++)if(!install(i,k_stubs[i])){
   for(j=0;j<i;j++)(void)uninstall(j);return 0;
 }
 if(!vmt_set(fhd_player_react_orig,FHDNPC_PlayerReactStub)){
   for(j=0;j<REQUIRED_SITES;j++)(void)uninstall(j);return 0;
 }
 fhd_clear_item=b+CLEAR_ITEM_RVA;
 fhd_relation_continue=b+RELATION_VALUE_RVA+6u;
 fhd_forminfo_continue=b+FORM_INFO_RVA+9u;
 fhd_relation_value=b+RELATION_VALUE_RVA;
 for(i=REQUIRED_SITES;i<SITE_COUNT;i++)
   optional_installed[i-REQUIRED_SITES]=(uint8_t)install(i,k_stubs[i]);
 if(optional_installed[3])
   fhd_relation_value=(void*)FHDNPC_RelationRawTrampoline;
 g_armed=1u;
 return 1;
}
int32_t FHDNPC_DisarmGrievance(void){uint32_t i;
 if(!g_armed)return 0;
 (void)vmt_set(FHDNPC_PlayerReactStub,fhd_player_react_orig);
 for(i=0;i<REQUIRED_SITES;i++)(void)uninstall(i);
 for(i=REQUIRED_SITES;i<SITE_COUNT;i++)if(optional_installed[i-REQUIRED_SITES]){
   (void)uninstall(i);optional_installed[i-REQUIRED_SITES]=0;
 }
 g_armed=0;return 1;
}
uint32_t FHDNPC_GrievanceCapabilities(void){
 return (optional_installed[0]&&optional_installed[1]?1u:0u)|(optional_installed[2]?2u:0u);
}

/* Experimental pilot personality description in TFormInfo (toggleable flag). */
static uint32_t g_show_character_info=1u;
API void FHDNPC_SetShowCharacterInfo(uint32_t enabled){g_show_character_info=enabled?1u:0u;}
static uint32_t w_append(uint16_t *dst,uint32_t cap,uint32_t n,const uint16_t *s){
 if(!s)return n;
 for(uint32_t i=0;s[i]&&n+1u<cap;i++)dst[n++]=s[i];
 dst[n]=0;return n;
}
static uint32_t w_append_i32(uint16_t *dst,uint32_t cap,uint32_t n,int32_t val){
 uint16_t tmp[16];uint32_t len=0;uint32_t u;
 if(val<0){if(n+1u<cap)dst[n++]='-';u=(uint32_t)(-val);}
 else {if(val>0 && n+1u<cap)dst[n++]='+';u=(uint32_t)val;}
 do{tmp[len++]=(uint16_t)('0'+(u%10u));u/=10u;}while(u&&len<12u);
 while(len && n+1u<cap)dst[n++]=tmp[--len];
 dst[n]=0;return n;
}
void FHDNPC_AppendShipTraitInfo(void *ship,uint16_t **p_text){
 static const uint16_t *const k_chars[11]={
   u"Бандит",u"Громила",u"Корсар",u"Делец",u"Магнат",
   u"Гангстер",u"Рейдер",u"Торгаш",u"Наёмник",u"Воин",u"Герой"
 };
 static const uint16_t *const k_traits[10]={
   u"отважный",u"агрессивный",u"жадный",u"осторожный",u"честный",
   u"хитрый",u"мстительный",u"верный",u"общительный",u"любознательный"
 };
 const uint8_t *s=(const uint8_t*)ship;
 FHDProfile prof;
 if(!g_armed||!g_show_character_info||!s||!p_text||!loot_actor(s))return;
 uint32_t id=read32(s,4u),seed=read32(s,0xe0u);
 if(FHD_ProfileOfIdentity(id,seed,&prof)!=1)return;
 /* Pick top 2 strongest personality traits for a concise, readable summary. */
 uint32_t t1=0,t2=1;
 if(prof.traits[t2]>prof.traits[t1]){t1=1;t2=0;}
 for(uint32_t i=2u;i<10u;i++){
   if(prof.traits[i]>prof.traits[t1]){t2=t1;t1=i;}
   else if(prof.traits[i]>prof.traits[t2]){t2=i;}
 }
 uint16_t buf[1024];uint32_t n=0u;
 if(*p_text)n=w_append(buf,1024u,n,*p_text);
 n=w_append(buf,1024u,n,u"\r\n<color=130,210,255>Характер:</color> ");
 n=w_append(buf,1024u,n,prof.character<11u?k_chars[prof.character]:u"Вольный пилот");
 n=w_append(buf,1024u,n,u" (");
 n=w_append(buf,1024u,n,k_traits[t1]);
 n=w_append(buf,1024u,n,u", ");
 n=w_append(buf,1024u,n,k_traits[t2]);
 n=w_append(buf,1024u,n,u")");
 uint32_t live=read32((const void*)g_base,ENCODED_PLAYER_RVA)^PLAYER_XOR_KEY;
 const uint8_t *pl=(const uint8_t*)(uintptr_t)live;
 if(pl && player(pl)){
   int32_t rel=FHD_RelationPeer(id,read32(pl,4u),read32(pl,0xe0u));
   if(rel!=0){
     n=w_append(buf,1024u,n,rel>0?u" | Память: ":u" | Обида: ");
     n=w_append_i32(buf,1024u,n,rel);
   }
 }
 uint16_t *fresh=SysAllocStringLen(buf,n);
 if(!fresh)return;
 if(*p_text)SysFreeString(*p_text);
 *p_text=fresh;
}
