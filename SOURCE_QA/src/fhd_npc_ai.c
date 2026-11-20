#include "fhd_npc_ai.h"
#include "fhd_profession_policy.h"
#include "fhd_world.h"
#include "fhd_dialogue.h"

/* No CRT, heap allocation, network, callbacks or background threads.
   All calls execute on the caller thread. Core does NOT hook the game. */
#pragma pack(push,1)
typedef struct Contact {
  uint32_t other_id, last_day, seed;
  int16_t relation;
  uint16_t event_count;
} Contact;
typedef struct Agent {
  uint32_t used, seed, last_day, active;
  FHDProfile profile;
  Contact contacts[FHD_MAX_RELATIONS];
  FHDTradeGoal trade;
  struct {uint32_t player_seed,star,day,side;} incident;
  int32_t trade_experience;
  FHDDuty duty;
  uint32_t trade_origin;
} Agent;
typedef struct SnapshotHeader {
  uint8_t magic[8];
  uint32_t abi, count, day, record_size, hash;
} SnapshotHeader;
typedef struct LegacyContact {
  uint32_t other_id, last_day;
  int16_t relation;
  uint16_t event_count;
} LegacyContact;
typedef struct LegacyAgent {
  uint32_t used, seed, last_day, active;
  FHDProfile profile;
  LegacyContact contacts[FHD_MAX_RELATIONS];
} LegacyAgent;
_Static_assert(sizeof(LegacyAgent)==232, "legacy snapshot size");
#define TRADE_SNAPSHOT_SIZE 336u
#define PRIOR_DUTY_SIZE 364u
#define WORLD_DUTY_SIZE 368u
_Static_assert(sizeof(Agent)==WORLD_DUTY_SIZE+4u, "current snapshot size");
#pragma pack(pop)

#define TABLE_SIZE 16384u
static Agent g_agents[TABLE_SIZE];
static uint32_t g_restore_ids[TABLE_SIZE]; /* collision-aware duplicate-ID check */
static uint32_t g_count, g_day;
/* Rebuilt from saved trade plans. Entries are agent indices rather than ship pointers. */
#define CARGO_BUCKETS 1024u
static uint16_t cargo_heads[CARGO_BUCKETS],cargo_next[TABLE_SIZE+1u],cargo_prev[TABLE_SIZE+1u];
static unsigned cargo_bucket(uint32_t planet,unsigned good){return (planet*31u+good)&(CARGO_BUCKETS-1);}
static void unlink_cargo(Agent *a){
  if(!a->trade.destination)return;
  unsigned i=(unsigned)(a-g_agents)+1, b=cargo_bucket(a->trade.destination,a->trade.good);
  unsigned before=cargo_prev[i],after=cargo_next[i];
  if(before)cargo_next[before]=(uint16_t)after;
  else if(cargo_heads[b]==i)cargo_heads[b]=(uint16_t)after;
  if(after)cargo_prev[after]=(uint16_t)before;
  cargo_prev[i]=cargo_next[i]=0;
}
static void link_cargo(Agent *a){
  if(!a->active||!a->trade.destination)return;
  unsigned i=(unsigned)(a-g_agents)+1,b=cargo_bucket(a->trade.destination,a->trade.good);
  unsigned head=cargo_heads[b];
  cargo_next[i]=(uint16_t)head;
  if(head)cargo_prev[head]=(uint16_t)i;
  cargo_heads[b]=(uint16_t)i;
}
unsigned FHD_IncomingCargo(uint32_t destination,unsigned good,uint32_t except_id,uint32_t day){
  unsigned total=0;
  for(unsigned i=cargo_heads[cargo_bucket(destination,good)];i;i=cargo_next[i]){
    const Agent *a=&g_agents[i-1];
    if(a->active&&a->used!=except_id&&a->trade.destination==destination&&
       a->trade.good==good&&a->trade.expires>=day)total+=a->trade.quantity;
  }
  return total;
}
static const uint8_t g_weights[11][3] = {
  {100,0,0}, {85,0,15}, {65,35,0}, {20,80,0}, {0,100,0},
  {70,0,30}, {40,25,35}, {15,70,15}, {15,25,60}, {0,15,85}, {0,0,100}
};
static uint32_t mix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
  x *= 0x846ca68bu; x ^= x >> 16; return x;
}
static uint8_t clamp100(int32_t x) { return (uint8_t)(x<0?0:(x>100?100:x)); }
static int32_t clamprel(int32_t x) { return x < -100 ? -100 : (x > 100 ? 100 : x); }
static uint32_t hash_bytes(const uint8_t *ptr, uint32_t size, uint32_t hash) {
  uint32_t i;
  for (i=0;i<size;i++) { hash ^= ptr[i]; hash *= 16777619u; }
  return hash;
}
static void clear_bytes(void *pointer, uint32_t count) {
  uint8_t *p=(uint8_t*)pointer; while(count--) *p++=0;
}
static void copy_bytes(void *dst,const void *src,uint32_t count) {
  uint8_t *d=(uint8_t*)dst;const uint8_t *s=(const uint8_t*)src;while(count--) *d++=*s++;
}
#define TOMBSTONE 0xffffffffu
static Agent* lookup(uint32_t id, int create) {
  uint32_t pos, i; Agent*first_tombstone=0;
  if(id==0 || id==TOMBSTONE) return 0;
  pos=mix32(id) & (TABLE_SIZE-1u);
  for(i=0;i<TABLE_SIZE;i++){
    Agent *a=&g_agents[(pos+i)&(TABLE_SIZE-1u)];
    if(a->used==id) return a;
    if(a->used==TOMBSTONE) { if(!first_tombstone) first_tombstone=a; continue; }
    if(a->used==0){
      if(!create || g_count>=FHD_MAX_AGENTS) return 0;
      if(first_tombstone)a=first_tombstone;
      clear_bytes(a,(uint32_t)sizeof(*a));a->used=id;g_count++;return a;
    }
  }
  if(create && first_tombstone && g_count<FHD_MAX_AGENTS){
    clear_bytes(first_tombstone,(uint32_t)sizeof(*first_tombstone));
    first_tombstone->used=id;g_count++;return first_tombstone;
  }
  return 0;
}
static uint32_t random_trait(uint32_t seed,uint32_t id,uint32_t salt){
  return mix32(seed ^ (id*2654435761u) ^ (salt*2246822519u)) % 101u;
}
static uint8_t character_by_weights(uint32_t p,uint32_t t,uint32_t m){
  uint32_t total=p+t+m, best=0xffffffffu, i; uint8_t result=FHD_PEDDLER;
  if(total==0) return result;
  p=(p*100u)/total;t=(t*100u)/total;m=100u-p-t;
  for(i=0;i<11;i++){
    int32_t dp=(int32_t)p-g_weights[i][0];
    int32_t dt=(int32_t)t-g_weights[i][1];
    int32_t dm=(int32_t)m-g_weights[i][2];
    uint32_t dist=(uint32_t)(dp*dp+dt*dt+dm*dm);
    if(dist<best){best=dist;result=(uint8_t)i;}
  }
  return result;
}
static void make_profile(Agent *a, uint32_t id, uint32_t seed,int32_t character,
                         uint32_t role, uint32_t p, uint32_t t,uint32_t m) {
  uint32_t i, total;
  static const uint32_t salts[10]={11,29,41,59,71,89,107,131,149,167};
  a->seed=seed;a->last_day=g_day;
  a->profile.id=id;
  a->profile.character=(uint8_t)(character>=0 && character<11 ? character : character_by_weights(p,t,m));
  a->profile.role=(uint8_t)(role>127?FHD_ROLE_MOD:role);
  total=g_weights[a->profile.character][0]+g_weights[a->profile.character][1]+g_weights[a->profile.character][2];
  if(total != 100) return;
  for(i=0;i<3;i++) a->profile.profession[i]=g_weights[a->profile.character][i];
  for(i=0;i<10;i++)a->profile.traits[i]=(uint8_t)random_trait(seed,id,salts[i]);
  /* Original focus influences personality, but identity is still unique. */
  a->profile.traits[1]=clamp100(((int)a->profile.traits[1]+(int)a->profile.profession[0])/2);
  a->profile.traits[2]=clamp100(((int)a->profile.traits[2]+(int)a->profile.profession[1])/2);
  a->profile.traits[0]=clamp100(((int)a->profile.traits[0]+(int)a->profile.profession[2])/2);
}
static uint32_t peer_seed(uint32_t other) {
  Agent *peer=lookup(other,0);
  return peer && peer->active ? peer->seed : 0;
}
/* Decay depends on elapsed time. Strong grudges cool more slowly than trust. */
static int32_t effective(const Contact *c,uint32_t day) {
  int32_t r=c->relation;
  uint32_t elapsed;
  if(!c->other_id || day<=c->last_day)return r;
  uint32_t age=day-c->last_day;
  uint32_t strong=r<0?(uint32_t)-r:(uint32_t)r;
  strong=strong>59?(strong-59)*(r<0?180u:120u):0;
  elapsed=age<strong?age/(r<0?180u:120u):strong/(r<0?180u:120u)+(age-strong)/30u;
  if(r>0)return elapsed>=(uint32_t)r ? 0 : r-(int32_t)elapsed;
  return elapsed>=(uint32_t)-r ? 0 : r+(int32_t)elapsed;
}
static Contact *contact(Agent*a,uint32_t other,uint32_t seed,int create) {
  uint32_t i,where=0;int32_t lowest=0x7fffffff;
  if(!other || other==a->used) return 0;
  for(i=0;i<FHD_MAX_RELATIONS;i++){
    Contact*c=&a->contacts[i];
    if(c->other_id==other){
      if(c->seed==seed)return c;
      if(!create)return 0;
      where=i;break;
    }
    if(!c->other_id) {where=i;lowest=-1;break;}
    /* Prefer to preserve intense relationships and recent interactions. */
    {
      int32_t r=effective(c,g_day);
      int32_t absrel=r<0?-r:r;
      uint32_t age=g_day>=c->last_day ? g_day-c->last_day : 0;
      int32_t rank=absrel*4096+4095-(int32_t)(age>4095u?4095u:age);
      if(rank<lowest){lowest=rank;where=i;}
    }
  }
  if(!create)return 0;
  clear_bytes(&a->contacts[where],(uint32_t)sizeof(Contact));
  a->contacts[where].other_id=other;a->contacts[where].seed=seed;return &a->contacts[where];
}

static int legacy_player_pending;
uint32_t FHD_CALL FHD_Version(void){return FHD_ABI_VERSION;}
void FHD_CALL FHD_Reset(void){
  clear_bytes(g_agents,sizeof(g_agents));clear_bytes(cargo_heads,sizeof(cargo_heads));
  clear_bytes(cargo_next,sizeof(cargo_next));clear_bytes(cargo_prev,sizeof(cargo_prev));
  FHD_WorldReset();FHDNPC_DialogueReset();g_count=0;g_day=0;legacy_player_pending=0;
}
uint32_t FHD_CALL FHD_Count(void){return g_count;}
int32_t FHD_CALL FHD_Upsert(uint32_t id,uint32_t seed,int32_t character,
  uint32_t role,uint32_t native_type,uint32_t flags,uint32_t p,uint32_t t,uint32_t m){
  Agent*a;
  /* Explicit conservative exclusion: dominator / Klissan / player / scripted. */
  if(native_type==FHD_SHIP_DOMINATOR || native_type==FHD_SHIP_TRANCLUCATOR || native_type==FHD_STATION_DOMINION ||
    (native_type>13u && !(flags & FHD_FLAG_CLASSIFIED)) || (flags &
    (FHD_FLAG_DOMINATOR|FHD_FLAG_KLISSAN|FHD_FLAG_PLAYER|FHD_FLAG_SCRIPT_LOCK|FHD_FLAG_STORY_LOCK))) {
    Agent *old=lookup(id,0);
    if(old){unlink_cargo(old);old->active=0;clear_bytes(&old->trade,sizeof(old->trade));old->trade_origin=0;}
    return 0;
  }
  if(!id || id==TOMBSTONE)return -1;
  p=p>100?100:p;t=t>100?100:t;m=m>100?100:m;
  a=lookup(id,1);if(!a)return -2;
  if(a->profile.id==0 || a->seed!=seed) {
    unlink_cargo(a);
    /* Reused ship id must never inherit the previous captain's contacts. */
    clear_bytes(a->contacts,(uint32_t)sizeof(a->contacts));
    clear_bytes(&a->trade,(uint32_t)sizeof(a->trade));
    a->trade_origin=0;
    clear_bytes(&a->incident,(uint32_t)sizeof(a->incident));
    a->trade_experience=0;
    clear_bytes(&a->duty,(uint32_t)sizeof(a->duty));
    clear_bytes(&a->profile,(uint32_t)sizeof(a->profile));
    make_profile(a,id,seed,character,role,p,t,m);
  }
  else a->profile.role=(uint8_t)(role>127?FHD_ROLE_MOD:role);
  a->active=1;
  return 1;
}
int32_t FHD_CALL FHD_Forget(uint32_t id){
  Agent*a=lookup(id,0);
  if(!a)return 0;
  unlink_cargo(a);
  clear_bytes(a,(uint32_t)sizeof(*a));a->used=TOMBSTONE;g_count--;
  return 1;
}
int32_t FHD_CALL FHD_ProfileOf(uint32_t id,FHDProfile*out){
  Agent*a=lookup(id,0);if(!out||!a||!a->active)return 0;
  copy_bytes(out,&a->profile,(uint32_t)sizeof(*out));return 1;
}
int32_t FHD_ProfileOfIdentity(uint32_t id,uint32_t seed,FHDProfile *out){
  Agent *a=lookup(id,0);
  if(!a||!a->active||a->seed!=seed)return 0;
  if(out)*out=a->profile;
  return 1;
}
int32_t FHD_RelationPeer(uint32_t id,uint32_t other,uint32_t seed){
  Agent*a=lookup(id,0);Contact*c;
  if(!a||!a->active)return 0;
  c=contact(a,other,seed,0);
  return c?effective(c,g_day):0;
}
int32_t FHD_AdjustRecordedPeer(uint32_t id,uint32_t other,uint32_t seed,int32_t value,uint32_t day){
 Agent*a=lookup(id,0);Contact*c;
 if(!a||!a->active||!other||id==other)return 0;
 c=contact(a,other,seed,0);if(!c)return 0;
 if(day>g_day)g_day=day;
 c->relation=(int16_t)clamprel(value);
 c->last_day=g_day;
 return 1;
}
int32_t FHD_CALL FHD_Relation(uint32_t id,uint32_t other){
  return FHD_RelationPeer(id,other,peer_seed(other));
}
uint32_t FHD_RelationPeerLastDay(uint32_t id,uint32_t other,uint32_t seed){
  Agent*a=lookup(id,0);Contact*c;
  if(!a||!a->active)return 0;
  c=contact(a,other,seed,0);return c?c->last_day:0;
}
uint32_t FHD_CALL FHD_RelationLastDay(uint32_t id,uint32_t other){
  return FHD_RelationPeerLastDay(id,other,peer_seed(other));
}
uint32_t FHD_FirstStrike(uint32_t id,uint32_t seed,uint32_t player_seed,uint32_t star,uint32_t day){
  const Agent *a=lookup(id,0);
  if(!a||!a->active||a->seed!=seed||!star||a->incident.star!=star||
     a->incident.player_seed!=player_seed||day<a->incident.day||day-a->incident.day>15u)return 0;
  return a->incident.side;
}
void FHD_NoteFirstStrike(uint32_t id,uint32_t seed,uint32_t player_seed,uint32_t star,
                         uint32_t day,uint32_t side){
  Agent *a=lookup(id,0);
  if(!a||!a->active||a->seed!=seed||!star||(side!=1u&&side!=2u)||
     FHD_FirstStrike(id,seed,player_seed,star,day))return;
  a->incident.player_seed=player_seed;a->incident.star=star;
  a->incident.day=day;a->incident.side=side;
}
int32_t FHD_GetTradeGoal(uint32_t id,FHDTradeGoal *goal){
  Agent *a=lookup(id,0);
  if(!goal||!a||!a->active||!a->trade.destination)return 0;
  *goal=a->trade;
  return 1;
}
int32_t FHD_TradeExperience(uint32_t id){
  Agent *a=lookup(id,0);
  return a&&a->active?a->trade_experience:0;
}
int FHD_GetDuty(uint32_t id,FHDDuty *out){
  Agent *a=lookup(id,0);
  if(!a||!a->active||!a->duty.kind)return 0;
  *out=a->duty;
  return 1;
}
void FHD_SetDuty(uint32_t id,const FHDDuty *duty){
  Agent *a=lookup(id,0);
  if(!a||!a->active)return;
  if(duty)a->duty=*duty;
  else clear_bytes(&a->duty,(uint32_t)sizeof(a->duty));
}
void FHD_SetTradeGoal(uint32_t id,const FHDTradeGoal *goal){
  Agent *a=lookup(id,0);
  if(!a||!a->active)return;
  unlink_cargo(a);
  if(goal){a->trade=*goal;link_cargo(a);}
  else {clear_bytes(&a->trade,(uint32_t)sizeof(a->trade));a->trade_origin=0;}
}
void FHD_SetTradeOrigin(uint32_t id,uint32_t planet){
  Agent *a=lookup(id,0);
  if(a&&a->active&&a->trade.destination)a->trade_origin=planet;
}
uint32_t FHD_TradeOrigin(uint32_t id){
  Agent *a=lookup(id,0);
  return a&&a->active?a->trade_origin:0;
}
void FHD_BindLegacyPlayer(uint32_t id,uint32_t seed){
  if(!legacy_player_pending||!id)return;
  legacy_player_pending=0;
  for(uint32_t i=0;i<TABLE_SIZE;i++)if(g_agents[i].active){
    Contact *c=contact(&g_agents[i],id,0,0);
    if(c)c->seed=seed;
  }
}
/* No changes to state, SAV serialization or behavior. Order-independent
   accumulator ensures the same relationships produce the same fingerprint
   after a snapshot is loaded into a fresh hash table. */
int32_t FHD_CALL FHD_RelationAudit(uint32_t*out,uint32_t words){
 uint32_t i,j,k,active=0,events=0,links=0,pos=0,neg=0,sum=0x51f2d4a1u,xorhash=0;
 if(!out||words<6u)return 0;
 for(i=0;i<TABLE_SIZE;i++){
  const Agent*a=&g_agents[i];
  if(!a->used||a->used==TOMBSTONE||!a->active)continue;
  active++;events+=a->profile.events;
  k=mix32(a->used ^ (a->seed*2654435761u) ^ a->profile.events);
  sum+=k; xorhash^=(k>>3); /* order-independent */
  for(j=0;j<FHD_MAX_RELATIONS;j++){
   const Contact*contact=&a->contacts[j];uint32_t h;
   int32_t relation=effective(contact,g_day);
   if(!relation)continue;
   links++;
   if(relation>0)pos++;else neg++;
   h=mix32(a->used ^ (contact->other_id*2246822519u)
     ^ ((uint32_t)relation*3266489917u) ^ contact->seed
     ^ (contact->last_day*668265263u) ^ contact->event_count);
   sum+=h;xorhash^=(h>>5);
  }
 }
 out[0]=active;out[1]=events;out[2]=links;out[3]=pos;out[4]=neg;out[5]=sum^xorhash;
 return 1;
}
int32_t FHD_RecordPeer(uint32_t id,uint32_t other,uint32_t seed,uint32_t event,uint32_t magnitude,uint32_t day){
  Agent*a=lookup(id,0); Contact*c; int32_t delta=0,shift=0;
  if(!a||!a->active||!event||event>FHD_EVENT_LOOT_DESTROYED)return 0;
  if(day>g_day)g_day=day;
  if(magnitude>10)magnitude=10;
  if(!magnitude)magnitude=1;
  switch(event){
    case FHD_EVENT_HELP: delta=9;break;
    case FHD_EVENT_ATTACK: delta=-15;break;
    case FHD_EVENT_BETRAY: delta=-27;break;
    case FHD_EVENT_VICTORY: case FHD_EVENT_DEFEAT: break;
    case FHD_EVENT_PROFIT:
      if(a->trade_experience<20)a->trade_experience++;
      break;
    case FHD_EVENT_LOSS:
      if(a->trade_experience>-20)a->trade_experience--;
      break;
    case FHD_EVENT_TRADE: delta=3;break;
    case FHD_EVENT_EXPLORE:break;
    case FHD_EVENT_RESCUE:delta=13;break;
    case FHD_EVENT_DIPLOMACY:delta=5;break;
    case FHD_EVENT_EXTORTION:delta=-12;break;
    case FHD_EVENT_LOOT_TAKEN:delta=-7;break;
    case FHD_EVENT_LOOT_DESTROYED:delta=-13;break;
  }
  a->profile.events++;
  if(delta&&other) {
    c=contact(a,other,seed,1);if(c){shift=delta*(int32_t)magnitude/2;
      if(!shift)shift=(delta>0?1:-1);
      c->relation=(int16_t)clamprel(effective(c,g_day)+shift);
      c->last_day=g_day;if(c->event_count<65535)c->event_count++;
    }
  }
  return 1;
}
int32_t FHD_CALL FHD_Record(uint32_t id,uint32_t other,uint32_t event,uint32_t magnitude,uint32_t day){
  return FHD_RecordPeer(id,other,peer_seed(other),event,magnitude,day);
}
void FHD_CALL FHD_AdvanceDay(uint32_t day){
  if(day>g_day)g_day=day;
}
static void bid(int32_t *best,uint32_t *act,int32_t score,uint32_t action){
  if(score>*best){*best=score;*act=action;}
}
int32_t FHD_ChoosePeer(uint32_t id,uint32_t seed,const FHDSituation*s,FHDDecision*out){
  Agent*a=lookup(id,0);FHDProfile *p;int32_t best=-2147483647,rel,score;uint32_t act=FHD_ACTION_NONE;
  if(!a||!a->active||!s||!out)return 0;
  p=&a->profile;rel=FHD_RelationPeer(id,s->target_id,seed);
  /* Every option is grounded in host-supplied availability. Execution remains native.
     Native mission/script priorities MUST be enforced by the bridge. */
  if(s->trading_available){
    score=20+p->profession[1]+(int32_t)p->traits[2]/2+(int32_t)s->expected_profit/2-(int32_t)s->danger;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_TRADE);
    bid(&best,&act,score,FHD_ACTION_TRADE);
  }
  if(s->piracy_available){
    score=10+p->profession[0]+(int32_t)p->traits[1]/2+(int32_t)p->traits[5]/3
         - (int32_t)s->relative_enemy_power -(int32_t)p->traits[4]/3
         + (rel<0?(-rel)/2:0) - (rel>0?rel:0);
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_PIRACY);
    bid(&best,&act,score,FHD_ACTION_PIRACY);
  }
  if(s->combat_available){
    score=12+p->profession[2]+(int32_t)p->traits[0]/2+(int32_t)p->traits[1]/3
         -(int32_t)s->relative_enemy_power -(int32_t)s->danger/2
         +(rel<0?(-rel)/3:0) -(rel>0?rel*2:0);
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_FIGHT);
    bid(&best,&act,score,FHD_ACTION_FIGHT);
  }
  if(s->ally_in_danger && s->combat_available){
    score=25+p->profession[2]+(int32_t)p->traits[7]+(int32_t)p->traits[4]/3
         -(int32_t)s->relative_enemy_power/2;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_DEFEND_ALLY);
    bid(&best,&act,score,FHD_ACTION_DEFEND_ALLY);
  }
  if(s->repair_available && s->hull_damage){
    score=30+(int32_t)p->traits[3]/2 + (int32_t)s->hull_damage*2;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_REPAIR);
    bid(&best,&act,score,FHD_ACTION_REPAIR);
  }
  if(s->flee_available && s->danger){
    score=15+(int32_t)p->traits[3]+(int32_t)s->danger
         -(int32_t)p->traits[0]/2;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_FLEE);
    bid(&best,&act,score,FHD_ACTION_FLEE);
  }
  if(s->explore_available){
    score=24+(int32_t)p->traits[9]/2-(int32_t)s->danger/2;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_EXPLORE);
    bid(&best,&act,score,FHD_ACTION_EXPLORE);
  }
  if(s->diplomacy_available){
    score=20+(int32_t)p->traits[8]/2+(int32_t)p->traits[4]/2
         +(rel>0?rel/2:0);
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_DIPLOMACY);
    bid(&best,&act,score,FHD_ACTION_DIPLOMACY);
  }
  if(s->mission_available){
    score=30+(int32_t)p->traits[7]/2+(int32_t)p->traits[9]/3;
    score+=FHDNPC_RoleDecisionBonus(p,FHD_ACTION_MISSION);
    bid(&best,&act,score,FHD_ACTION_MISSION);
  }
  out->action=act;out->score=(act==FHD_ACTION_NONE?0:best);
  out->relation_to_target=rel;return 1;
}
int32_t FHD_CALL FHD_Choose(uint32_t id,const FHDSituation*s,FHDDecision*out){
  return FHD_ChoosePeer(id,s?peer_seed(s->target_id):0,s,out);
}
static void read_agent(Agent *agent,const uint8_t *source,uint32_t size){
  if(size!=sizeof(LegacyAgent)){
    clear_bytes(agent,(uint32_t)sizeof(*agent));
    copy_bytes(agent,source,size);
    return;
  }
  LegacyAgent old;uint32_t i;
  copy_bytes(&old,source,(uint32_t)sizeof(old));
  clear_bytes(agent,(uint32_t)sizeof(*agent));
  agent->used=old.used;agent->seed=old.seed;agent->last_day=old.last_day;
  agent->active=old.active;agent->profile=old.profile;
  for(i=0;i<FHD_MAX_RELATIONS;i++){
    const LegacyContact *c=&old.contacts[i];
    agent->contacts[i]=(Contact){c->other_id,c->last_day,0,c->relation,c->event_count};
  }
}
/* Snapshot: header + exact packed records. Local binary format, not game SAV. */
int32_t FHD_CALL FHD_Snapshot(void *buffer,uint32_t capacity){
  SnapshotHeader h;uint32_t need=(uint32_t)sizeof(h)+g_count*(uint32_t)sizeof(Agent)+FHD_WORLD_BYTES+FHD_DIALOGUE_HISTORY_BYTES;
  uint32_t i, offset=(uint32_t)sizeof(h), hash=2166136261u;
  if(!buffer)return (int32_t)need;
  if(capacity<need)return -1;
  clear_bytes(&h,(uint32_t)sizeof(h));
  h.magic[0]='F';h.magic[1]='H';h.magic[2]='D';h.magic[3]='N';
  h.magic[4]='P';h.magic[5]='C';h.magic[6]='0';h.magic[7]='8';
  h.abi=FHD_ABI_VERSION;h.count=g_count;h.day=g_day;h.record_size=(uint32_t)sizeof(Agent);
  for(i=0;i<TABLE_SIZE;i++)if(g_agents[i].used && g_agents[i].used!=TOMBSTONE){
    copy_bytes((uint8_t*)buffer+offset,&g_agents[i],(uint32_t)sizeof(Agent));
    hash=hash_bytes((uint8_t*)buffer+offset,(uint32_t)sizeof(Agent),hash);
    offset+=(uint32_t)sizeof(Agent);
  }
  copy_bytes((uint8_t*)buffer+offset,FHD_WorldBytes(),FHD_WORLD_BYTES);
  hash=hash_bytes((uint8_t*)buffer+offset,FHD_WORLD_BYTES,hash);
  offset+=FHD_WORLD_BYTES;
  copy_bytes((uint8_t*)buffer+offset,FHD_DialogueHistory(),FHD_DIALOGUE_HISTORY_BYTES);
  h.hash=hash_bytes((uint8_t*)buffer+offset,FHD_DIALOGUE_HISTORY_BYTES,hash);
  copy_bytes(buffer,&h,(uint32_t)sizeof(h));return (int32_t)need;
}
int32_t FHD_CALL FHD_Restore(const void *buffer,uint32_t size){
  SnapshotHeader h;uint32_t need,i,hash=2166136261u;int legacy;
  const uint8_t *ptr=(const uint8_t*)buffer;
  if(!ptr||size<(uint32_t)sizeof(h))return -1;
  copy_bytes(&h,ptr,(uint32_t)sizeof(h));
  if(h.magic[0]!='F'||h.magic[1]!='H'||h.magic[2]!='D'||h.magic[3]!='N'||
     h.magic[4]!='P'||h.magic[5]!='C'||h.magic[6]!='0'||h.magic[7]<'3'||h.magic[7]>'8'||
     h.abi!=FHD_ABI_VERSION||h.count>FHD_MAX_AGENTS)return -2;
  legacy=h.magic[7]=='3';
  if(h.record_size!=(h.magic[7]=='3'?sizeof(LegacyAgent):
      h.magic[7]=='4'?TRADE_SNAPSHOT_SIZE:h.magic[7]=='5'?PRIOR_DUTY_SIZE:
      h.magic[7]=='6'?WORLD_DUTY_SIZE:sizeof(Agent)))return -2;
  need=(uint32_t)sizeof(h)+h.count*h.record_size;
  unsigned world_size=h.magic[7]>='7'?FHD_WORLD_BYTES:h.magic[7]=='6'?FHD_WORLD_V6_BYTES:0u;
  unsigned history_size=h.magic[7]=='8'?FHD_DIALOGUE_HISTORY_BYTES:0u;
  if(size!=need+world_size+history_size)return -3;
  hash=hash_bytes(ptr+(uint32_t)sizeof(h),size-(uint32_t)sizeof(h),hash);
  if(hash!=h.hash)return -4;
  if(world_size&&!FHD_WorldValidate(ptr+need,world_size))return -6;
  /* Full validation before altering existing state. */
  clear_bytes(g_restore_ids,(uint32_t)sizeof(g_restore_ids));
  for(i=0;i<h.count;i++){
    Agent a;uint32_t j;
    read_agent(&a,ptr+sizeof(h)+i*h.record_size,h.record_size);
    uint32_t pos, n, sum=0;
    if(!a.used||a.used==TOMBSTONE||a.profile.id!=a.used||a.profile.character>10||a.active>1||
       a.profile.role>127)return -5;
    for(j=0;j<10;j++)if(a.profile.traits[j]>100)return -5;
    for(j=0;j<3;j++)sum+=a.profile.profession[j];
    if(sum!=100)return -5;
    pos=mix32(a.used)&(TABLE_SIZE-1u);
    for(n=0;n<TABLE_SIZE;n++){
      uint32_t*id=&g_restore_ids[(pos+n)&(TABLE_SIZE-1u)];
      if(*id==a.used)return -7;
      if(!*id){*id=a.used;break;}
    }
    if(n==TABLE_SIZE)return -7;
    if(a.incident.side>2u || (a.incident.side && !a.incident.star))return -6;
    if(a.trade_experience<-20||a.trade_experience>20)return -6;
    if(a.duty.kind>FHD_DUTY_REQUEST||(a.duty.kind&&(!a.duty.star||!a.duty.expires))||
       a.duty.phase>1u||
       a.duty.fee>2000u||
       ((a.duty.kind==FHD_DUTY_CLIENT||a.duty.kind==FHD_DUTY_ESCORT)&&
        (!a.duty.peer||!a.duty.planet)))return -6;
    if(a.trade.destination && (a.trade.good>7u||!a.trade.quantity||
       !a.trade.unit_cost||a.trade.unit_cost>1000000u||a.trade.quantity>200u))return -6;
    for(j=0;j<FHD_MAX_RELATIONS;j++){
      if(a.contacts[j].relation < -100 || a.contacts[j].relation >100)return -6;
    }
  }
  FHD_Reset();g_day=h.day;
  if(world_size)FHD_WorldLoad(ptr+need,world_size);
  if(history_size)FHD_DialogueRestoreHistory(ptr+need+world_size);
  for(i=0;i<h.count;i++){
    Agent a,*slot;
    read_agent(&a,ptr+sizeof(h)+i*h.record_size,h.record_size);
    if(!world_size&&(a.duty.kind==FHD_DUTY_CLIENT||a.duty.kind==FHD_DUTY_ESCORT))
      clear_bytes(&a.duty,sizeof(a.duty)); /* Old contracts held no escrow. */
    slot=lookup(a.used,1);
    if(!slot)return -7;
    copy_bytes(slot,&a,(uint32_t)sizeof(a));
    link_cargo(slot);
  }
  legacy_player_pending=legacy;
  if(legacy){
    for(i=0;i<TABLE_SIZE;i++)if(g_agents[i].active){
      for(uint32_t j=0;j<FHD_MAX_RELATIONS;j++){
        Contact *c=&g_agents[i].contacts[j];
        if(c->other_id)c->seed=peer_seed(c->other_id);
      }
    }
  }
  return (int32_t)g_count;
}
#if defined(_WIN32)
/* Detach restores the original VMT slots before a hot-disabled DLL unload. */
extern void FHDNPC_OnDllDetach(void);
int __stdcall DllMain(void *module,unsigned long reason,void *reserved){
  (void)module;
  if(reason==0 && !reserved) FHDNPC_OnDllDetach();
  return 1;
}
#endif
