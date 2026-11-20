/* Native rule adapter 0.8.0. No arbitrary game function calls, no orders,
 * no UI interception and no save-file manipulation. Uses verified 2026
 * Delphi TShip / TRanger layout. Only TRanger preferences are modified,
 * strictly during its original NextDayLogic; other categories are observed.
 */
#include "fhd_live_policy.h"
#include "fhd_profession_policy.h"
#include <stdint.h>
/* No CRT: copy raw IEEE754 bits without float pointer aliasing. */
#if defined(_WIN32)
int _fltused = 0;  /* MSVC i686 floating point runtime marker */
#endif
#define MAX_TRACKED 16384u
static FHDLiveStats s;
static uint32_t g_profession_seen[15];
void FHDNPC_ProfessionStats(uint32_t*out){uint32_t i;if(out)for(i=0;i<10u;i++)out[i]=g_profession_seen[i];}
void FHDNPC_ExtraProfessionStats(uint32_t*out){uint32_t i;if(out)for(i=0;i<5u;i++)out[i]=g_profession_seen[i+10u];}
/* V1.4 diagnostic counters do not extend the fixed FHDLiveStats ABI. */
static struct {uint32_t friends,enemies,direct_applied,busy_overridden;
               uint32_t pirate_up,pirate_down,ranger_up,ranger_down;
               uint32_t unchanged,blocked_fear;} social14;
void FHDNPC_Live140Stats(uint32_t*out){uint32_t i;
  const uint32_t *p=(const uint32_t*)&social14;
  if(out)for(i=0;i<10u;i++)out[i]=p[i];
}

/* Last confirmed observation, not just the start of the current NextDayLogic.
   It catches ship events occurring in the flight/combat simulation between
   two NPC decision calls. Replaced on a reused id with a different seed. */
static struct { uint32_t id,seed; int32_t kills; } g_observed[MAX_TRACKED];
static uint32_t rd32(const uint8_t *p,uint32_t o){
 uint32_t x;uint8_t *d=(uint8_t*)&x;uint32_t i;
 for(i=0;i<4;i++)d[i]=p[o+i];return x;
}
static int32_t rdi32(const uint8_t*p,uint32_t o){return (int32_t)rd32(p,o);}
static void z(void *dst,uint32_t n){uint8_t*p=(uint8_t*)dst;while(n--)*p++=0;}
static void cp(void*d,const void*s0,uint32_t n){uint8_t *p=(uint8_t*)d;const uint8_t*q=(const uint8_t*)s0;while(n--)*p++=*q++;}
void FHDNPC_LiveReset(void){z(&social14,(uint32_t)sizeof(social14));z(g_profession_seen,(uint32_t)sizeof(g_profession_seen));z(&s,(uint32_t)sizeof(s));z(g_observed,(uint32_t)sizeof(g_observed));FHDNPC_ResetVerifiedProfessions();}
void FHDNPC_LiveStatsGet(FHDLiveStats*out){if(out)cp(out,&s,(uint32_t)sizeof(s));}
static void observe_delta(uint32_t id,uint32_t seed,uint32_t day,uint32_t category,
                          int32_t kills){
 uint32_t i,j=((id*2654435761u)^seed)&(MAX_TRACKED-1u);
 for(i=0;i<MAX_TRACKED;i++){
   uint32_t k=(j+i)&(MAX_TRACKED-1u);
   if(!g_observed[k].id || g_observed[k].id==id){
     if(g_observed[k].id==id && g_observed[k].seed==seed){
       int32_t oldkills=g_observed[k].kills;
       if(category<4u && kills>oldkills && kills-oldkills<=100){
         FHD_Record(id,0,FHD_EVENT_VICTORY,(uint32_t)(kills-oldkills),day);
         s.confirmed_kills+=(uint32_t)(kills-oldkills);
       }
     }
     g_observed[k].id=id;g_observed[k].seed=seed;
     g_observed[k].kills=kills;
     return;
   }
 }
}
static int allow(const uint8_t *p,uint32_t c){
 uint8_t owner,type;
 if(!p||c>=6u){s.skip_invalid++;return 0;}
 owner=p[0x11u];type=p[0x10u];
 if(!(owner<=4u||owner==7u)||type==0u||type==12u||type>13u){s.skip_provenance++;return 0;}
 if(c==4u||(c==3u&&p[0x510u]==1u)){s.skip_special++;return 0;}
 if(c<5u&&type!=(c+1u)){s.skip_provenance++;return 0;}
 if(c==5u&&(type<6u||type>11u)){s.skip_provenance++;return 0;}
 if(rd32(p,0x0cu)!=0u){s.skip_provenance++;return 0;} /* unknown mod origin */
 if(rd32(p,0x3ccu)!=0u){s.skip_scripts++;return 0;}
 if(p[0x44cu]||p[0x43cu]||p[0x458u]||p[0x461u]||p[0x49cu]){
   s.skip_special++;return 0;
 }
 return 1;
}
static uint32_t role_of(uint32_t c){
 static const uint32_t roles[]={FHD_ROLE_RANGER,FHD_ROLE_TRANSPORT,FHD_ROLE_PIRATE,
 FHD_ROLE_MILITARY,FHD_ROLE_SPECIAL,FHD_ROLE_STATION};return roles[c];
}
extern void FHDNPC_ObservePartnership(void *ship,uint32_t category);
extern uint32_t FHDNPC_VerifiedRelationIdentity(void *ship,void *target,uint32_t *seed);
extern uint32_t FHDNPC_ObservedThreat(void *ship,void *target);
void FHDNPC_LiveBefore(uint8_t *p,uint32_t c,FHDLiveFrame *f){
 FHDNpcDescriptor n;FHDProfile prof;FHDSituation sit;FHDDecision dec,neutral;
 uint32_t id,seed,day,pp=0,tt=0,mm=0,act=0,profession=0;
 uint32_t relation_changed=0,rel_active=0,rel_direct=0,target_seed=0;
 int32_t status,relationship=0;
 if(!f)return;z(f,(uint32_t)sizeof(*f));s.considered++;
 if(!allow(p,c)){s.excluded++;return;}
 id=rd32(p,4);seed=rd32(p,0xe0u);day=rd32(p,0xecu);
 profession=FHDNPC_LookupVerifiedProfession(id,seed,p[0x10u]);
 /* Restore a previously verified role from this captain's saved profile. */
 if(!profession&&c<4u&&FHD_ProfileOfIdentity(id,seed,&prof)){
   switch(prof.role){
     case FHD_ROLE_AGENT:profession=FHD_PROF_VERIFIED_AGENT;break;
     case FHD_ROLE_SPECIAL_AGENT:profession=FHD_PROF_VERIFIED_SPECIAL_AGENT;break;
     case FHD_ROLE_SMUGGLER:profession=FHD_PROF_VERIFIED_SMUGGLER;break;
     case FHD_ROLE_MERCENARY:profession=FHD_PROF_VERIFIED_MERCENARY;break;
     case FHD_ROLE_SPY:profession=FHD_PROF_VERIFIED_SPY;break;
   }
 }
 if(!profession)profession=FHDNPC_Profession(p[0x10u],c==2u?p[0x514u]:
   ((c==1u||c==3u)?p[0x510u]:0u));
 if(!profession){s.excluded++;s.skip_profile++;return;}
 if(profession<15u)g_profession_seen[profession]++;
 if(!id||id==0xffffffffu||day>1000000u){s.excluded++;s.skip_identity++;return;}
 if(c==0u){tt=p[0x513u];pp=p[0x514u];mm=p[0x515u];}
 else if(c==1u||c==5u){tt=90;mm=10;}
 else if(c==2u){pp=90;mm=10;}
 else {mm=90;tt=10;}
 z(&n,(uint32_t)sizeof(n));
 n.id=id;n.seed=seed;n.native_type=p[0x10u];n.owner=(p[0x11u]==7u?FHD_OWNER_PIRATES:FHD_OWNER_COALITION);
 n.role=FHDNPC_RoleFromProfession(profession);
 if(n.role==FHD_ROLE_UNKNOWN)n.role=role_of(c);
 n.origin=FHD_ORIGIN_VANILLA;n.character=-1;
 n.pirate=pp;n.trade=tt;n.military=mm;
 status=FHD_BridgeUpsert(&n);
 if(status!=1){s.excluded++;s.skip_profile++;return;}
 s.registered++;s.categories[c]++;
 if(day)FHD_BridgeDay(day);
 /* Contracts are checked after successful native identity registration. */
 FHDNPC_ObservePartnership(p,c);
 f->id=id;f->seed=seed;f->day=day;f->money=rdi32(p,0xf0u);
 f->kills=c<4u?rdi32(p,0x4d4u):0;
 f->valid=1;
 observe_delta(id,seed,day,c,f->kills);
 if(FHD_ProfileOf(id,&prof)!=1){s.skip_profile++;return;}
 z(&sit,(uint32_t)sizeof(sit));
 if(rd32(p,0x3f4u)!=0u){
    unsigned threat=FHDNPC_ObservedThreat(p,(void *)(uintptr_t)rd32(p,0x3f4u));
    sit.combat_available=1;sit.danger=(uint8_t)(5+threat/2);
    sit.relative_enemy_power=(uint8_t)threat;sit.piracy_available=(c==0u||c==2u);
    if(c==0u||c==2u){
      sit.target_id=FHDNPC_VerifiedRelationIdentity(p,
                   (void*)(uintptr_t)rd32(p,0x3f4u),&target_seed);
      if(sit.target_id){
        s.relationship_target_eligible++;
        relationship=FHD_RelationPeer(id,sit.target_id,target_seed);
        if(relationship){
          s.relationship_target_nonzero++;
          rel_active=1;
          if(relationship>0)social14.friends++;else social14.enemies++;
        }
      }
    }
 }
 if(p[0x3b4u]){sit.danger=90;sit.flee_available=1;}
 if(rd32(p,0x1cu)!=0u||rd32(p,0x20u)!=0u){sit.trading_available=(c==0u||c==1u);}
 else {sit.explore_available=1;sit.trading_available=(c==0u||c==1u);}
 if(FHD_ChoosePeer(id,target_seed,&sit,&dec)!=1){s.skip_no_action++;return;}
 /* Compare with exactly the same situation but no relationship target.
    The extra call reads the same core state and cannot mutate a native order. */
 if(relationship){
   sit.target_id=0;
   if(FHD_Choose(id,&sit,&neutral)==1 && neutral.action!=dec.action){
     relation_changed=1;s.relationship_action_changed++;
   }
 }
 act=dec.action;if(act<=9u)s.action_counts[act]++;
 if(!act){s.skip_no_action++;return;}
 s.category_choices[c]++;
 f->action=act;
 /* Only rangers and pirates use these temporary preference fields. */
 if(c!=0u&&c!=2u){s.skip_unsupported++;return;}
 if(c==0u)s.ranger_choices++;
 /* Do not alter NPCs with ongoing absolute orders, partnerships, or target orders. */
 if(rd32(p,0x3fcu)||rd32(p,0x430u)||rd32(p,0x3d0u)){s.skip_busy++;return;}
 if(c==2u){
  /* Direct relation pressure applies even if the top-scoring core action did
     not switch. This removes the old all-or-nothing PIRACY gate for verified
     enemies/friends, without issuing a native order or touching other fields. */
  uint32_t b=rd32(p,0x518u),i;
  float pressure,adjusted;
  cp(&pressure,&b,4u);
  if((b&0x7f800000u)==0x7f800000u||pressure<0.0f||pressure>15.0f){
    s.skip_invalid_pressure++;return;
  }
  adjusted=pressure;
  if(rel_active && relationship>=15){
    adjusted-=0.50f+(float)(relationship>100?100:relationship)*0.014f;
    social14.pirate_down++;rel_direct=1;
  }else if(rel_active && relationship<=-15 && !p[0x3b4u]){
    adjusted+=0.40f+(float)(relationship< -100?100:-relationship)*0.012f;
    social14.pirate_up++;rel_direct=1;
  }else if(!p[0x3b4u] && act==FHD_ACTION_PIRACY){
    adjusted+=0.25f+(float)prof.traits[1]*0.0125f;
  }else{
    if(p[0x3b4u])social14.blocked_fear++;
    s.skip_no_effect++;return;
  }
  if(adjusted>15.0f)adjusted=15.0f;
  if(adjusted<0.0f)adjusted=0.0f;
  cp(&i,&adjusted,4u);
  if(i==b){s.skip_no_effect++;social14.unchanged++;return;}
  f->old_pressure_bits=b;f->used_pressure_bits=i;
  cp(p+0x518u,&i,4u);f->pirate_changed=1;s.changed_pirates++;s.category_applied[2u]++;
  if(rel_direct)social14.direct_applied++;
  if(relation_changed)s.relationship_action_applied++;
  return;
 }
 if(p[0x51cu]>2u||p[0x51du]>100u){s.skip_invalid_preferences++;return;}
 f->old_career=p[0x51cu];f->old_aggression=p[0x51du];
 f->used_career=f->old_career;f->used_aggression=f->old_aggression;
 switch(act){
  case FHD_ACTION_TRADE: f->used_career=0;f->used_aggression=(uint8_t)(15u+prof.traits[1]/4u);break;
  case FHD_ACTION_PIRACY: f->used_career=1;f->used_aggression=(uint8_t)(60u+prof.traits[1]/3u);break;
  case FHD_ACTION_FIGHT: case FHD_ACTION_DEFEND_ALLY:
    f->used_career=2;f->used_aggression=(uint8_t)(50u+prof.traits[0]/2u);break;
  case FHD_ACTION_FLEE: f->used_aggression=(uint8_t)(prof.traits[0]/4u);break;
  default:
    if(!rel_active || (relationship>-15 && relationship<15)){
      s.skip_no_effect++;return;
    }
    /* Direct relationship may affect native preferences while vanilla
       situation chooses an otherwise unsupported noncombat action. */
    break;
 }
 /* A verified active friendship/deep grievance can influence actual
    temporary native preferences even when FHD_Choose picked the same action.
    Restoring the originals after NextDayLogic remains mandatory. */
 if(rel_active && relationship>=15){
   f->used_career=0u;
   f->used_aggression=(uint8_t)(10u+(uint32_t)(100-relationship)/5u);
   social14.ranger_down++;rel_direct=1;
 }else if(rel_active && relationship<=-15){
   uint32_t value=65u+(uint32_t)(-relationship)/3u;
   f->used_career=(f->old_career==1u)?1u:2u;
   f->used_aggression=(uint8_t)(value>100u?100u:value);
   social14.ranger_up++;rel_direct=1;
 }
 if(f->old_career!=f->used_career||f->old_aggression!=f->used_aggression){
  p[0x51cu]=f->used_career;p[0x51du]=f->used_aggression;
  f->changed=1;s.ranger_applied++;s.category_applied[0u]++;
  if(rel_direct)social14.direct_applied++;
  if(relation_changed)s.relationship_action_applied++;
 } else {s.skip_no_effect++;if(rel_direct)social14.unchanged++;}
}
void FHDNPC_LiveAfter(uint8_t*p,uint32_t c,FHDLiveFrame*f){
 if(!p||!f||!f->valid||c>=6u)return;
 if(rd32(p,4)!=f->id||rd32(p,0xe0u)!=f->seed)return;
 if(c==0u&&f->changed){
  /* Avoid undoing a genuine game-side update to these same fields. */
  if(p[0x51cu]==f->used_career)p[0x51cu]=f->old_career;
  else s.game_writes_preserved++;
  if(p[0x51du]==f->used_aggression)p[0x51du]=f->old_aggression;
  else s.game_writes_preserved++;
 }
 if(c==2u&&f->pirate_changed){
   uint32_t current=rd32(p,0x518u);
   if(current==f->used_pressure_bits)cp(p+0x518u,&f->old_pressure_bits,4u);
   else s.game_writes_preserved++;
 }
 observe_delta(f->id,f->seed,f->day,c,
               c<4u?rdi32(p,0x4d4u):0);
}
