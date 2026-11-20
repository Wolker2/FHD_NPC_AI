#include "fhd_profession_policy.h"
uint32_t FHDNPC_Profession(uint32_t type,uint32_t sub){
 switch(type){
  case 1u:return FHD_PROF_RANGER;
  case 2u:
   if(sub==0u)return FHD_PROF_TRADER;
   if(sub==1u)return FHD_PROF_LINER;
   if(sub==2u)return FHD_PROF_DIPLOMAT;
   return FHD_PROF_UNKNOWN;
  case 3u:return sub==0u?FHD_PROF_FREE_PIRATE:FHD_PROF_CLAN_PIRATE;
  case 4u:
   if(sub==0u)return FHD_PROF_WARRIOR;
   if(sub==1u)return FHD_PROF_FLAGSHIP;
   return FHD_PROF_UNKNOWN;
  case 6u:case 7u:case 8u:case 9u:case 10u:case 11u:
   return FHD_PROF_STATION;
  default:return FHD_PROF_UNKNOWN;
 }
}
uint32_t FHDNPC_RoleFromProfession(uint32_t prof){
 switch(prof){
  case FHD_PROF_RANGER:return FHD_ROLE_RANGER;
  case FHD_PROF_TRADER:return FHD_ROLE_TRADER;
  case FHD_PROF_LINER:return FHD_ROLE_LINER;
  case FHD_PROF_DIPLOMAT:return FHD_ROLE_DIPLOMAT;
  case FHD_PROF_FREE_PIRATE:case FHD_PROF_CLAN_PIRATE:return FHD_ROLE_PIRATE;
  case FHD_PROF_WARRIOR:case FHD_PROF_FLAGSHIP:return FHD_ROLE_MILITARY;
  case FHD_PROF_STATION:return FHD_ROLE_STATION;
  case FHD_PROF_VERIFIED_AGENT:return FHD_ROLE_AGENT;
  case FHD_PROF_VERIFIED_SPECIAL_AGENT:return FHD_ROLE_SPECIAL_AGENT;
  case FHD_PROF_VERIFIED_SMUGGLER:return FHD_ROLE_SMUGGLER;
  case FHD_PROF_VERIFIED_MERCENARY:return FHD_ROLE_MERCENARY;
  case FHD_PROF_VERIFIED_SPY:return FHD_ROLE_SPY;
  default:return FHD_ROLE_UNKNOWN;
 }
}
uint32_t FHDNPC_PirateMotive(const FHDProfile*p,int32_t rel,
 uint32_t sub,int32_t money,uint32_t cargo_present,
 uint32_t retaliation,uint32_t is_player){
 /* Retaliation to a witnessed attack always has priority over friendship.
    A clan fighter retains defense of its territory (not necessarily robbery). */
 if(!p)return FHD_PIRATE_NO_VERIFIED_MOTIVE;
 if(retaliation || rel<=-15)return FHD_PIRATE_REVENGE;
 if(rel>=15)return FHD_PIRATE_PROTECT_FRIEND;
 if(sub!=0u)return FHD_PIRATE_CLAN_DUTY;
 /* A demand for money is economically motivated only when target money exists.
    Game's own TryExtortShip still decides between cash/cargo and dialogue. */
 if((money>=200 || cargo_present) &&
    (p->traits[2]>=25u || p->traits[5]>=60u || is_player))
  return FHD_PIRATE_RANSOM_OR_CARGO;
 return FHD_PIRATE_NO_VERIFIED_MOTIVE;
}
uint32_t FHDNPC_AvoidStrongPrey(const FHDProfile*p,uint32_t motive,
 uint32_t own_strength,uint32_t enemy_strength){
 if(!p || motive!=FHD_PIRATE_RANSOM_OR_CARGO ||
    own_strength==0u || enemy_strength==0u)return 0;
 return p->traits[5]>=65u && p->traits[1]<80u &&
        (uint64_t)enemy_strength >= (uint64_t)own_strength*2u;
}
uint32_t FHDNPC_InterruptHop(uint32_t hops,uint32_t has_enemy,
 uint32_t armed,uint32_t own_strength,uint32_t enemy_strength){
 if(hops<3u||!has_enemy||!armed||!own_strength||!enemy_strength)return 0;
 /* Do not force a hopeless or unknown battle. Strength is a native cached
    Single translated into comparable bounded integer units by host. */
 return (uint64_t)own_strength*3u >= (uint64_t)enemy_strength*2u;
}

/* The real game still controls the escape order, destination, truce request,
   repairs and trade. This policy only biases the pre-existing fear state when
   the same previously witnessed attacker is still in the star. */
uint32_t FHDNPC_MemoryFear(uint32_t profession,const FHDProfile *p,
 int32_t relation,uint32_t own,uint32_t enemy,uint32_t hull){
 uint64_t threat;
 if(!p || relation>-7 || !own || !enemy || hull>100u)return 0;
 threat=(uint64_t)enemy*100u;
 switch(profession){
  case FHD_PROF_LINER: return threat>=(uint64_t)own*70u || hull<=65u;
  case FHD_PROF_DIPLOMAT: return threat>=(uint64_t)own*80u || hull<=55u;
  case FHD_PROF_TRADER: return threat>=(uint64_t)own*110u ||
    (threat>=(uint64_t)own*85u && (p->traits[3]>=35u || hull<=60u));
  case FHD_PROF_VERIFIED_SMUGGLER:
  case FHD_PROF_VERIFIED_SPY: return threat>=(uint64_t)own*75u || hull<=50u;
  case FHD_PROF_VERIFIED_AGENT:
  case FHD_PROF_VERIFIED_SPECIAL_AGENT: return threat>=(uint64_t)own*95u && p->traits[3]>=45u;
  case FHD_PROF_VERIFIED_MERCENARY: return threat>=(uint64_t)own*150u && p->traits[3]>=55u;
  default:return 0;
 }
}
uint32_t FHDNPC_AttackMemoryWeight(uint32_t profession){
 switch(profession){
  case FHD_PROF_LINER:case FHD_PROF_DIPLOMAT:return 2u;
  case FHD_PROF_VERIFIED_SPY:case FHD_PROF_VERIFIED_SPECIAL_AGENT:return 2u;
  default:return 1u;
 }
}
uint32_t FHDNPC_TrustKnownAggressor(uint32_t original_trust,
 int32_t relation,uint32_t profession){
 if(!original_trust)return 0;
 if(profession==FHD_PROF_UNKNOWN)return original_trust;
 return relation<=-7?0u:original_trust;
}
/* Strict explicit tags. No ship address is retained: ship object pointers can
   be recycled by the engine after a load. Only vanilla-compatible ordinary
   x86 TShip types may be tagged; bosses/aliens/custom station types cannot. */
#define VERIFIED_CAPACITY 4096u
static struct {uint32_t id,seed,type,profession;} verified[VERIFIED_CAPACITY];
static uint32_t tagslot(uint32_t id,uint32_t seed){
 return (id*2654435761u ^ seed*2246822519u)&(VERIFIED_CAPACITY-1u);
}
int32_t FHDNPC_RegisterVerifiedProfession(uint32_t id,uint32_t seed,
 uint32_t type,uint32_t prof){
 uint32_t i,index;
 if(!id || id==0xffffffffu || !seed || type<1u || type>4u ||
    prof<FHD_PROF_VERIFIED_AGENT || prof>FHD_PROF_VERIFIED_SPY)return 0;
 index=tagslot(id,seed);
 for(i=0;i<VERIFIED_CAPACITY;i++){
  uint32_t j=(index+i)&(VERIFIED_CAPACITY-1u);
  if(!verified[j].id || (verified[j].id==id && verified[j].seed==seed)){
   verified[j].id=id;verified[j].seed=seed;
   verified[j].type=type;verified[j].profession=prof;return 1;
  }
 }
 return 0;
}
uint32_t FHDNPC_LookupVerifiedProfession(uint32_t id,uint32_t seed,uint32_t type){
 uint32_t i,index;
 if(!id || !seed || type<1u || type>4u)return 0;
 index=tagslot(id,seed);
 for(i=0;i<VERIFIED_CAPACITY;i++){
  uint32_t j=(index+i)&(VERIFIED_CAPACITY-1u);
  if(!verified[j].id)return 0;
  if(verified[j].id==id && verified[j].seed==seed && verified[j].type==type)
    return verified[j].profession;
 }
 return 0;
}
void FHDNPC_ResetVerifiedProfessions(void){
 uint32_t i;for(i=0;i<VERIFIED_CAPACITY;i++)verified[i].id=0u;
}

/* Every option must first be advertised by the actual game/mod executor.
   The scorer cannot create a mission, new native order, secret trade or spy job. */
int32_t FHDNPC_RoleDecisionBonus(const FHDProfile *p,uint32_t action){
 uint32_t cunning;
 if(!p)return 0;
 cunning=p->traits[5];
 switch(p->role){
  case FHD_ROLE_AGENT:
    if(action==FHD_ACTION_MISSION)return 28;
    if(action==FHD_ACTION_DIPLOMACY)return 18+(int32_t)cunning/5;
    if(action==FHD_ACTION_FIGHT)return -12;
    return 0;
  case FHD_ROLE_SPECIAL_AGENT:
    if(action==FHD_ACTION_MISSION)return 35;
    if(action==FHD_ACTION_DEFEND_ALLY)return 20;
    if(action==FHD_ACTION_FIGHT)return 8;
    return 0;
  case FHD_ROLE_SPY:
    if(action==FHD_ACTION_EXPLORE)return 25+(int32_t)cunning/5;
    if(action==FHD_ACTION_DIPLOMACY)return 24+(int32_t)cunning/5;
    if(action==FHD_ACTION_FLEE)return 16+(int32_t)cunning/5;
    if(action==FHD_ACTION_FIGHT)return -35;
    if(action==FHD_ACTION_PIRACY)return -45;
    return 0;
  case FHD_ROLE_SMUGGLER:
    if(action==FHD_ACTION_TRADE)return 26;
    if(action==FHD_ACTION_PIRACY)return 16+(int32_t)cunning/6;
    if(action==FHD_ACTION_FLEE)return 12+(int32_t)cunning/7;
    return 0;
  case FHD_ROLE_MERCENARY:
    if(action==FHD_ACTION_MISSION)return 30;
    if(action==FHD_ACTION_FIGHT)return 22;
    if(action==FHD_ACTION_DEFEND_ALLY)return 24;
    if(action==FHD_ACTION_TRADE)return -20;
    return 0;
  default:return 0;
 }
}
