/* Native daily routines retain maintenance, combat, and locked orders.
   These dispatchers act at the vanilla discretionary travel branches. */
#include "fhd_native_duties.h"
#include "fhd_npc_ai.h"
#include "fhd_world.h"
#include "fhd_dialogue.h"
#include <stdint.h>
#if defined(FHD_DUTIES_TEST) && defined(__i386__)
#define WIN
#define ST
#elif defined(_WIN32) && defined(_M_IX86)
#define WIN __declspec(dllimport)
#define ST __stdcall
#else
#error Win32 x86 only
#endif
#define REG __attribute__((regparm(3)))
WIN void * ST GetModuleHandleW(const uint16_t*);
WIN void * ST GetCurrentProcess(void);
WIN int ST VirtualProtect(void *,uint32_t,uint32_t,uint32_t *);
WIN int ST FlushInstructionCache(void *,const void *,uint32_t);

enum {
  GALAXY=0x41d2a0, REPAIR=0x2c9a68, PLANET_RELATION=0x339578,
  ORDER_JUMP=0x2e27ac, ORDER_MOVE=0x2e2560,
  SET_MONEY=0x2c7938, ATTACKING=0x2d38d4, JOINT_ATTACK=0x2d3ec8,
  STAR_LIMIT=12, PLANET_LIMIT=12, SHIP_LIMIT=48
};
enum {ID=4, TYPE=0x10, OWNER=0x11, PLANET=0x1c, DOCK=0x20, STAR=0x24,
      RANGE=0xc8, SEED=0xe0, MONEY=0xf0, FEAR=0x3b4, SCRIPT=0x3cc,
      GROUP=0x3d0, PICKUP=0x3d8, ENEMY=0x3f4, PARTNER=0x3fc,
      ORDER=0x428, TARGET=0x430, ABSOLUTE=0x43c, SCRIPT_ORDER=0x44c,
      LAST_PLANET=0x4d0};
typedef struct {void *vmt;uint8_t **items;int count;} List;
typedef struct {int distance;uint8_t *star;} Distance;
typedef void (REG *Idle)(void *);
typedef void (REG *RangerIdle)(void *,unsigned);
typedef uint8_t (REG *Check)(void *);
typedef uint8_t (REG *Relation)(void *,void *);
typedef void (REG *JointAttack)(void *,void *,void *);
typedef void (REG *Move)(void *,const void *,unsigned);
typedef void (REG *SetMoney)(void *,int);
typedef struct {uint32_t rva;uint8_t bytes[5];} Signature;

static uint8_t *image;
static unsigned armed;
static uint32_t stats[12]; /* scout, patrol, raid, liner, envoy, escort, station, paid, failed, arrived, defended, requests */
static const uint32_t ship_vmt[]={0,0x310c70,0x1ab58c,0x1992b0,0x1a2b10,0,0x29a3a8};
static uint32_t read32(const uint8_t *p,unsigned offset){
  p+=offset;
  return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void *ref(const uint8_t *p,unsigned offset){return (void *)(uintptr_t)read32(p,offset);}
static uint32_t id(const uint8_t *p){return p?read32(p,ID):0;}
static uint32_t seed(const uint8_t *p){return read32(p,SEED);}
static uint8_t *world(void){return ref(image,GALAXY);}
static uint32_t day(void){uint8_t *w=world();return w?read32(w,0x4c):0;}
static int min(int a,int b){return a<b?a:b;}
static int list_count(const List *l,int cap){return l&&l->items?min(l->count,cap):0;}
static int ordinary(const uint8_t *p){
  if(!p||!id(p)||p[TYPE]>11u||p[OWNER]==5u||p[OWNER]==6u||p[OWNER]>7u)return 0;
  unsigned type=p[TYPE]<=5u?p[TYPE]:6u;
  return ship_vmt[type]&&ref(p,0)==image+ship_vmt[type]&&!ref(p,0x0c)&&!ref(p,SCRIPT)&&
      !p[0x458]&&!p[0x460]&&!p[0x461]&&!p[0x49c]&&!(type==4u&&p[0x510]==1u);
}
static int normal(const uint8_t *p){return ref(p,STAR)&&!ref(p,PLANET)&&!ref(p,DOCK)&&!p[0x459];}
static int nearby_ship(const uint8_t *a,const uint8_t *b){
  union {float f;uint32_t u;} ax,ay,bx,by;
  ax.u=read32(a,0x14);ay.u=read32(a,0x18);
  bx.u=read32(b,0x14);by.u=read32(b,0x18);
  float dx=ax.f-bx.f,dy=ay.f-by.f;
  return dx*dx+dy*dy<=1500.0f*1500.0f;
}
static int idle(const uint8_t *p){
  const List *loot=ref(p,PICKUP);
  return normal(p)&&!p[ORDER]&&!p[ABSOLUTE]&&!p[SCRIPT_ORDER]&&!p[FEAR]&&
     !ref(p,GROUP)&&!ref(p,PARTNER)&&!ref(p,ENEMY)&&!list_count(loot,1);
}
static int system_allowed(const uint8_t *s){
  const uint8_t *sector=s?ref(s,0xd0):0;
  return sector&&id(sector)!=20&&!ref(s,0x44);
}
static int peaceful(const uint8_t *s){return system_allowed(s)&&s[0x42]==0&&!s[0x48];}
static int landable(uint8_t *p,uint8_t *ship){
  return p&&p[0x74]<=4&&!p[0x15c]&&!ref(p,0x164)&&
      ((Relation)(image+PLANET_RELATION))(p,ship)>=25;
}
static const Distance *neighbors(uint8_t *star,int *count){
  uint8_t *w=world();
  *count=w?list_count(ref(w,0x2c),STAR_LIMIT):0;
  return star?ref(star,0x94):0;
}
static uint8_t *near_star(uint8_t *from,uint32_t star_id){
  if(id(from)==star_id)return from;
  int count;const Distance *near=neighbors(from,&count);
  if(near)for(int i=0;i<count;i++)if(id(near[i].star)==star_id)return near[i].star;
  return 0;
}
static int within_range(uint8_t *ship,uint8_t *target){
  uint8_t *from=ref(ship,STAR);
  if(target==from)return 1;
  int count;const Distance *near=neighbors(from,&count);
  if(near)for(int i=0;i<count;i++)
    if(near[i].star==target)return near[i].distance>=0&&near[i].distance<=(int)read32(ship,RANGE);
  return 0;
}
static uint8_t *find_peer(uint8_t *star,uint32_t key,uint32_t peer_seed){
  const List *ships=star?ref(star,0x2c):0;
  for(int i=0;i<list_count(ships,SHIP_LIMIT);i++){
    uint8_t *peer=ships->items[i];
    if(id(peer)==key&&ordinary(peer)&&seed(peer)==peer_seed)return peer;
  }
  return 0;
}
static uint8_t *find_planet(uint8_t *star,uint32_t key){
  const List *planets=star?ref(star,0x24):0;
  for(int i=0;i<list_count(planets,PLANET_LIMIT);i++)if(id(planets->items[i])==key)return planets->items[i];
  return 0;
}
static int issue(uint8_t *p,uint8_t *target,unsigned order){
  ((Move)(image+(order==3?ORDER_JUMP:ORDER_MOVE)))(p,target,0);
  return p[ORDER]==order&&(order==1||ref(p,TARGET)==target);
}
static void cancel(uint8_t *p){
  FHDDuty d;
  if(FHD_GetDuty(id(p),&d)&&d.kind==FHD_DUTY_CLIENT&&d.fee){
    unsigned cash=read32(p,MONEY),rest=100000000u-cash;
    ((SetMoney)(image+SET_MONEY))(p,(int)(cash+(d.fee<rest?d.fee:rest)));
  }
  FHD_SetDuty(id(p),0);stats[8]++;
}
static void set_route(uint8_t *p,unsigned kind,uint8_t *star,uint32_t planet,uint32_t today){
  FHDDuty d={kind,id(star),planet,0,0,today+45,0,0};
  FHD_SetDuty(id(p),&d);
}

static int escort(uint8_t *p,uint32_t today,int defend_only){
  FHDDuty d;
  if(!FHD_GetDuty(id(p),&d)||d.kind!=FHD_DUTY_ESCORT)return 0;
  uint8_t *from=ref(p,STAR),*dest=near_star(from,d.star);
  if(today>d.expires){cancel(p);return 0;}
  uint8_t *client=find_peer(from,d.peer,d.peer_seed);
  if(!client&&dest&&dest!=from)client=find_peer(dest,d.peer,d.peer_seed);
  if(!client)return 0; /* Transit or temporary separation. */
  FHDDuty contract;
  if(!client||!FHD_GetDuty(id(client),&contract)||contract.kind!=FHD_DUTY_CLIENT||
     contract.peer!=id(p)||contract.peer_seed!=seed(p)||contract.planet!=d.planet||
     contract.star!=d.star||contract.expires<today){
    cancel(p);return 0;
  }
  uint8_t *enemy=ref(client,ENEMY);
  if(!d.phase&&ref(client,STAR)==from&&normal(client)&&nearby_ship(p,client)){
    d.phase=1;
    FHD_SetDuty(id(p),&d);
  }
  if(ref(client,STAR)==from&&normal(client)&&ordinary(enemy)&&enemy!=p&&
     normal(enemy)&&ref(enemy,STAR)==from&&ref(p,0x3f8)!=enemy&&ref(enemy,0x3f8)!=p&&
     ref(client,0x3f8)!=enemy&&ref(enemy,0x3f8)!=client&&
     p[0x12d]&&FHD_RelationPeer(id(p),id(enemy),seed(enemy))<15&&
     ((Relation)(image+ATTACKING))(enemy,client)){
    ((JointAttack)(image+JOINT_ATTACK))(p,p,enemy);
    if(ref(p,ENEMY)==enemy){
      d.phase=1;FHD_SetDuty(id(p),&d);stats[10]++;return 1;
    }
  }
  if(defend_only)return 0;
  if(ref(client,PLANET)&&contract.phase==0)return 0;
  if(dest&&system_allowed(dest)&&dest[0x42]==0&&from!=dest&&within_range(p,dest)&&
     (ref(client,STAR)==dest||(client[ORDER]==3&&ref(client,TARGET)==dest))){
    if(issue(p,dest,3)){stats[5]++;return 1;}
    return 0;
  }
  if(from==dest&&ref(client,STAR)!=from)return 0;
  if(ref(client,STAR)==from&&normal(client)&&!client[FEAR]&&!ref(client,ENEMY)){
    /* Neutral follow orders are cancelled by the engine on the next day.
       A local rendezvous uses a regular move order towards the client. */
    if(issue(p,client+0x14,1)){stats[5]++;return 1;}
  }
  return 0;
}
static int accept_escort(uint8_t *p,const FHDProfile *profile,uint32_t today){
  if(p[OWNER]>4||!p[0x12d]||profile->traits[0]+profile->traits[7]<75)return 0;
  uint8_t *from=ref(p,STAR);
  if(!system_allowed(from)||from[0x42]!=0)return 0;
  const List *ships=ref(from,0x2c);
  for(int i=0;i<list_count(ships,SHIP_LIMIT);i++){
    uint8_t *client=ships->items[i];
    FHDTradeGoal cargo;FHDDuty request;
    if(!ordinary(client)||client[TYPE]!=2||client[OWNER]>4||client[0x510]!=0||
       client[FEAR]||ref(client,ENEMY)||ref(client,GROUP)||ref(client,PARTNER)||
       client[ABSOLUTE]||client[SCRIPT_ORDER]||!ref(client,PLANET)||
       !FHD_ProfileOfIdentity(id(client),seed(client),0)||
       !FHD_GetTradeGoal(id(client),&cargo)||cargo.expires<today||
       !FHD_GetDuty(id(client),&request)||request.kind!=FHD_DUTY_REQUEST||
       request.expires<today||request.planet!=cargo.destination)continue;
    uint8_t *dest=near_star(from,request.star);
    unsigned balance=read32(client,MONEY);
    if(!peaceful(dest)||!within_range(p,dest)||!find_planet(dest,cargo.destination)||
       balance<request.fee+500u||
       FHD_RelationPeer(id(p),id(client),seed(client))<-7||
       FHD_RelationPeer(id(client),id(p),seed(p))<-7)continue;
    uint32_t expiry=min((int)cargo.expires,(int)(today+35));
    FHDDuty guard={FHD_DUTY_ESCORT,id(dest),cargo.destination,id(client),seed(client),expiry,0,request.fee};
    FHDDuty payer={FHD_DUTY_CLIENT,id(dest),cargo.destination,id(p),seed(p),expiry,0,request.fee};
    FHD_SetDuty(id(p),&guard);FHD_SetDuty(id(client),&payer);
    ((SetMoney)(image+SET_MONEY))(client,(int)(balance-request.fee));
    FHDNPC_LocalDialogue(FHD_DLG_CONTRACT,client,id(p),0,today,request.planet);
    stats[5]++;return 1;
  }
  return 0;
}
int FHDNPC_WaitForEscort(void *object){
  uint8_t *p=object;
  uint32_t today=armed?day():0;
  FHDTradeGoal trade;
  FHDDuty duty;
  if(!today||!ordinary(p)||p[TYPE]!=2||p[OWNER]>4||p[0x510]||!ref(p,PLANET)||
     p[FEAR]||p[ABSOLUTE]||p[SCRIPT_ORDER]||ref(p,GROUP)||ref(p,PARTNER)||
     !FHD_ProfileOfIdentity(id(p),seed(p),0)||!FHD_GetTradeGoal(id(p),&trade))return 0;
  if(FHD_GetDuty(id(p),&duty)){
    if(duty.kind==FHD_DUTY_CLIENT)
      return !duty.phase&&duty.expires>=today&&duty.planet==trade.destination&&
          ref(p,STAR)&&!find_peer(ref(p,STAR),duty.peer,duty.peer_seed);
    if(duty.kind!=FHD_DUTY_REQUEST)return 0;
    if(duty.expires>=today&&duty.planet==trade.destination)return 1;
    cancel(p);
    return 0;
  }
  uint8_t *from=ref(p,STAR);
  int count;const Distance *near=neighbors(from,&count);
  if(!near)return 0;
  for(int i=0;i<count;i++){
    uint8_t *dest=near[i].star;
    if(dest==from||!peaceful(dest)||!find_planet(dest,trade.destination))continue;
    unsigned risk=FHD_WorldRisk(id(dest),today);
    if(risk<25)return 0;
    unsigned fee=(unsigned)min(2000,100+(int)risk*4+(int)(trade.quantity*trade.unit_cost/50u));
    if(read32(p,MONEY)<fee+500u)return 0;
    FHDDuty request={FHD_DUTY_REQUEST,id(dest),trade.destination,0,0,today+3,0,fee};
    FHD_SetDuty(id(p),&request);
    stats[11]++;
    return 1;
  }
  return 0;
}
void FHDNPC_TradeCancelled(void *object){
  uint8_t *p=object;FHDDuty d;
  if(armed&&ordinary(p)&&FHD_GetDuty(id(p),&d)&&
     (d.kind==FHD_DUTY_REQUEST||d.kind==FHD_DUTY_CLIENT))cancel(p);
}
static uint8_t *distress_station(uint8_t *star,uint32_t today){
  const List *ships=ref(star,0x2c);
  for(int i=0;i<list_count(ships,SHIP_LIMIT);i++){
    uint8_t *p=ships->items[i];FHDDuty d;
    if(ordinary(p)&&p[TYPE]>=6&&p[TYPE]<=11&&p[OWNER]<=4&&
       FHD_ProfileOfIdentity(id(p),seed(p),0)&&FHD_GetDuty(id(p),&d)&&
       d.kind==FHD_DUTY_DISTRESS&&d.expires>=today)return p;
  }
  return 0;
}
static int score_star(const uint8_t *from,uint8_t *star,unsigned kind,int dist,
                      const FHDProfile *profile,uint32_t today,uint8_t **station){
  if(star==from||!system_allowed(star))return -1;
  if(kind==FHD_DUTY_RAID){
    if(!peaceful(star)||star[0x41]<10)return -1;
    return 40+star[0x41]+profile->traits[2]/4-dist*(2+profile->traits[3]/32)-
           star[0x40]-(int)FHD_WorldRisk(id(star),today)/2;
  }
  if(star[0x42]!=0)return -1;
  if(kind==FHD_DUTY_PATROL){
    *station=distress_station(star,today);
    unsigned risk=FHD_WorldRisk(id(star),today);
    if(!*station&&!star[0x48]&&star[0x40]<15&&risk<20)return -1;
    return 45+star[0x40]+(star[0x48]?25:0)+(*station?55:0)+(int)risk-dist*2;
  }
  if(star[0x48]||star[0x40]>35)return -1;
  if(profile->role==FHD_ROLE_AGENT||profile->role==FHD_ROLE_SPY)
    return 45+profile->traits[5]/3+star[0x41]/2-dist*2-star[0x40];
  return 55+profile->traits[9]/3-dist*2-star[0x40]-star[0x41]/2;
}
static int travel(uint8_t *p,unsigned kind,const FHDProfile *profile,uint32_t today){
  uint8_t *from=ref(p,STAR);FHDDuty d;
  if(!system_allowed(from)||from[0x42]==1)return 0;
  if(FHD_GetDuty(id(p),&d)){
    if(d.kind!=kind){cancel(p);return 0;}
    if(today>d.expires){cancel(p);return 0;}
    uint8_t *dest=near_star(from,d.star);
    if(dest==from){
      if(kind==FHD_DUTY_SCOUT){
        FHD_Record(id(p),0,FHD_EVENT_EXPLORE,1,today);
        FHD_SetDuty(id(p),0);stats[9]++;
      }else if(kind==FHD_DUTY_PATROL){
        uint8_t *station=d.peer?find_peer(from,d.peer,d.peer_seed):0;
        if(station&&!d.phase&&issue(p,station+0x14,1)){
          d.phase=1;
          FHD_SetDuty(id(p),&d);stats[1]++;return 1;
        }
        FHD_SetDuty(id(p),0);stats[9]++;
      }else if(kind==FHD_DUTY_RAID&&!d.phase){
        d.phase=1;FHD_SetDuty(id(p),&d);
        /* Give the existing pirate targeting/extortion a turn in this system. */
      }else if(kind==FHD_DUTY_RAID){
        uint8_t *haven=near_star(from,d.peer);
        if(haven&&haven!=from&&system_allowed(haven)&&!haven[0x48]&&
           haven[0x42]!=1&&within_range(p,haven)&&issue(p,haven,3)){
          FHD_SetDuty(id(p),0);stats[2]++;return 1;
        }
        cancel(p);
      }
      return 0;
    }
    if(dest&&system_allowed(dest)&&dest[0x42]==0&&
       (kind==FHD_DUTY_PATROL||peaceful(dest))&&within_range(p,dest)&&issue(p,dest,3))return 1;
    return 0;
  }
  if((seed(p)+today)%5u!=0)return 0;
  int count;const Distance *near=neighbors(from,&count);
  int best=0,range=(int)read32(p,RANGE);
  uint8_t *dest=0,*station=0;
  if(!near||range<=0)return 0;
  if(kind==FHD_DUTY_RAID&&(!p[0x12d]||!peaceful(from)||profile->traits[1]<35))return 0;
  for(int i=0;i<count;i++){
    int distance=near[i].distance;uint8_t *signal=0;
    if(distance<=0||distance>(kind==FHD_DUTY_RAID?range/2:range))continue;
    int score=score_star(from,near[i].star,kind,distance,profile,today,&signal);
    if(score>best){best=score;dest=near[i].star;station=signal;}
  }
  if(!dest||!issue(p,dest,3))return 0;
  FHDDuty chosen={kind,id(dest),0,station?id(station):kind==FHD_DUTY_RAID?id(from):0,
                 station?seed(station):0,today+35,0,0};
  FHD_SetDuty(id(p),&chosen);stats[kind-1]++;return 1;
}

static int mobile(uint8_t *p,unsigned category){
  uint32_t today=day();
  FHDProfile profile;FHDDuty d;
  if(!today||!ordinary(p)||p[TYPE]!=category||!idle(p)||!FHD_ProfileOfIdentity(id(p),seed(p),&profile)||
     ((Check)(image+REPAIR))(p))return 0;
  if(category!=3&&p[OWNER]>4)return 0;
  const List *quests=category==1?ref(p,0x520):0;
  if(list_count(quests,1))return 0;
  if(FHD_GetDuty(id(p),&d)&&d.expires<today){cancel(p);return 0;}
  if(category!=3){
    if(FHD_GetDuty(id(p),&d)&&d.kind==FHD_DUTY_ESCORT)return escort(p,today,0);
    if(!FHD_GetDuty(id(p),&d)&&(category==4||profile.role==FHD_ROLE_MERCENARY||
       profile.role==FHD_ROLE_SPECIAL_AGENT||profile.role==FHD_ROLE_RANGER)&&
       (seed(p)+today)%3u==0&&accept_escort(p,&profile,today))return 1;
  }
  unsigned kind=FHD_DUTY_SCOUT;
  if(category==3)kind=FHD_DUTY_RAID;
  else if(category==4||profile.role==FHD_ROLE_SPECIAL_AGENT||
          (profile.role==FHD_ROLE_RANGER&&profile.profession[2]>=60))kind=FHD_DUTY_PATROL;
  if(category==1&&kind==FHD_DUTY_SCOUT)
    for(unsigned good=0;good<8;good++)if(read32(p,0x30+16*good))return 0;
  return travel(p,kind,&profile,today);
}

void *FHDNPC_TransportDutyDestination(void *object){
  uint8_t *p=object,*from=ref(p,STAR);FHDDuty d;
  FHDProfile profile;
  uint32_t today=armed?day():0;
  if(!today||!ordinary(p)||p[TYPE]!=2||p[OWNER]>4||!idle(p)||!peaceful(from)||
     !FHD_ProfileOfIdentity(id(p),seed(p),&profile))return 0;
  unsigned kind=p[0x510]==1?FHD_DUTY_LINER:p[0x510]==2?FHD_DUTY_ENVOY:0;
  if(!kind||profile.role==FHD_ROLE_SMUGGLER)return 0;
  if(FHD_GetDuty(id(p),&d)){
    uint8_t *star=near_star(from,d.star);
    uint8_t *dest=star?find_planet(star,d.planet):0;
    if(d.kind!=kind||d.expires<today){cancel(p);return 0;}
    if(star&&peaceful(star)&&landable(dest,p)&&within_range(p,star))return dest;
    return 0;
  }
  int count;const Distance *near=neighbors(from,&count);
  int range=(int)read32(p,RANGE),best=0;
  uint8_t *dest=0,*dest_star=0;
  if(!near||range<0)return 0;
  for(int i=0;i<count;i++){
    uint8_t *star=near[i].star;int dist=near[i].distance;
    if(dist<0||dist>range||!peaceful(star))continue;
    const List *planets=ref(star,0x24);
    for(int j=0;j<list_count(planets,PLANET_LIMIT);j++){
      uint8_t *planet=planets->items[j];
      if(planet==ref(p,LAST_PLANET)||!landable(planet,p)||!id(planet))continue;
      int population=(int)read32(planet,0x68);
      int score=80+min(population/15000,65)+star[0x41]/2-
                star[0x40]*(1+profile.traits[3]/50)-dist*2;
      if(kind==FHD_DUTY_ENVOY)score+=planet[0x74]!=p[OWNER]?30+profile.traits[8]/5:0;
      if(score>best){best=score;dest=planet;dest_star=star;}
    }
  }
  if(dest){set_route(p,kind,dest_star,id(dest),today);stats[kind-1]++;}
  return dest;
}

static void finish_escort(uint8_t *client,const FHDDuty *d,uint32_t today){
  uint8_t *star=ref(client,STAR),*guard=find_peer(star,d->peer,d->peer_seed);
  FHDDuty theirs;
  if(!guard||(!normal(guard)&&!ref(guard,PLANET))||
     !FHD_GetDuty(id(guard),&theirs)||theirs.kind!=FHD_DUTY_ESCORT||
     !theirs.phase||theirs.expires<today||theirs.peer!=id(client)||theirs.peer_seed!=seed(client)||
     theirs.planet!=d->planet||theirs.star!=d->star)return;
  uint32_t guard_cash=read32(guard,MONEY),reward=d->fee;
  if(!reward||reward!=theirs.fee||guard_cash>100000000u-reward)return;
  FHD_SetDuty(id(client),0);FHD_SetDuty(id(guard),0);
  ((SetMoney)(image+SET_MONEY))(guard,(int)(guard_cash+reward));
  FHD_RecordPeer(id(client),id(guard),seed(guard),FHD_EVENT_HELP,2,today);
  FHD_RecordPeer(id(guard),id(client),seed(client),FHD_EVENT_DIPLOMACY,2,today);
  stats[7]++;
}
void FHDNPC_TradeDeliveryCompleted(void *object,void *location){
  uint8_t *p=object,*planet=location;
  FHDDuty d;
  uint32_t today=armed?day():0;
  if(!today||!ordinary(p)||!planet||!FHD_ProfileOfIdentity(id(p),seed(p),0)||
     !FHD_GetDuty(id(p),&d)||d.kind!=FHD_DUTY_CLIENT||d.expires<today||
     d.planet!=id(planet)||d.star!=id(ref(p,STAR)))return;
  d.phase=1;
  FHD_SetDuty(id(p),&d);
  finish_escort(p,&d,today);
}
static void negotiate_supply(uint8_t *envoy,uint8_t *destination,uint32_t today){
  uint8_t *origin=ref(envoy,0x2c);
  if(!origin||origin==destination||origin[0x74]>4||origin[0x74]==destination[0x74]||
     origin[0x15c]||ref(origin,0x164)||!peaceful(ref(origin,0x18))||!landable(origin,envoy))return;
  unsigned best=0,chosen=0;
  for(unsigned good=0;good<5;good++){
    unsigned offset=0x78+16u*good;
    int stock=(int)read32(origin,offset),need=(good==0?140:100)-(int)read32(destination,offset);
    int source_price=(int)read32(origin,offset+8),sale=(int)read32(destination,offset+12);
    if(stock<140||need<40||source_price<1||sale<=source_price||
       FHD_WorldAgreement(id(origin),id(destination),good,today))continue;
    unsigned score=(unsigned)min(stock-80,need);
    if(score>best){best=score;chosen=good;}
  }
  if(best&&FHD_WorldAgree(id(origin),id(destination),chosen,best,today))
    FHDNPC_LocalDialogue(FHD_DLG_AGREEMENT,envoy,id(destination),0,today,chosen);
}
void FHDNPC_DutyAfter(void *object,uint32_t category){
  uint8_t *p=object;FHDDuty d;
  uint32_t today=armed?day():0;
  if(!today||!ordinary(p)||!FHD_ProfileOfIdentity(id(p),seed(p),0))return;
  uint8_t *star=ref(p,STAR);
  if(!system_allowed(star))return;
  if(category==5){
    if(!normal(p)||p[OWNER]>4||star[0x42]!=0||p[ABSOLUTE]||p[SCRIPT_ORDER])return;
    if(star[0x48]||star[0x40]>=30||FHD_WorldRisk(id(star),today)>=25){
      if(!FHD_GetDuty(id(p),&d)||d.expires<today){
        set_route(p,FHD_DUTY_DISTRESS,star,0,today);stats[6]++;
        /* Do not fabricate public station bulletins from abstract world risk.
           Official reports are emitted only after real observed attacks. */
      }
    }else if(FHD_GetDuty(id(p),&d)&&d.kind==FHD_DUTY_DISTRESS)FHD_SetDuty(id(p),0);
    return;
  }
  if(!FHD_GetDuty(id(p),&d))return;
  if(d.kind==FHD_DUTY_CLIENT&&d.phase){finish_escort(p,&d,today);if(d.expires>=today)return;}
  if(d.expires<today&&d.kind!=FHD_DUTY_REQUEST){cancel(p);return;}
  if(d.kind==FHD_DUTY_ESCORT&&(category==0||category==3)&&p[ORDER]==1&&normal(p)&&
     !p[ABSOLUTE]&&!p[SCRIPT_ORDER]&&!p[FEAR]&&!ref(p,GROUP)&&!ref(p,PARTNER)&&
     !ref(p,ENEMY)&&!list_count(ref(p,PICKUP),1)){
    escort(p,today,1);
    return;
  }
  uint8_t *planet=ref(p,PLANET);
  if(category!=1||!planet||id(planet)!=d.planet||id(star)!=d.star)return;
  if(p[ABSOLUTE]||p[SCRIPT_ORDER]||ref(p,GROUP)||ref(p,PARTNER)||!peaceful(star)||
     !landable(planet,p))return;
  if(d.kind==FHD_DUTY_LINER||d.kind==FHD_DUTY_ENVOY){
    if(d.kind==FHD_DUTY_ENVOY)negotiate_supply(p,planet,today);
    FHD_SetDuty(id(p),0);
    stats[9]++;
  }
}

static void REG ranger_idle(void *p,unsigned mode){
  if(!mobile(p,1))((RangerIdle)(image+0x315a34))(p,mode);
}
static void REG warrior_orbit(void *p){
  if(!mobile(p,4))((Idle)(image+0x1a9d88))(p);
}
static uint8_t REG warrior_home(void *p){
  return mobile(p,4)?1:((Check)(image+0x1a4cd4))(p);
}
static void REG pirate_idle(void *p){
  if(!mobile(p,3))((Idle)(image+0x2cc900))(p);
}
static void REG clan_idle(void *p){
  if(!mobile(p,3))((Idle)(image+0x1a0834))(p);
}
static const struct {Signature sig;const void *replacement;} sites[]={
  {{0x313247,{0xe8,0xe8,0x27,0,0}},ranger_idle},
  {{0x1a3d91,{0xe8,0xf2,0x5f,0,0}},warrior_orbit},
  {{0x1a3d9b,{0xe8,0x34,0x0f,0,0}},warrior_home},
  {{0x19aa7e,{0xe8,0x7d,0x1e,0x13,0}},pirate_idle},
  {{0x19aa74,{0xe8,0xbb,0x5d,0,0}},clan_idle}
};
static const Signature routines[]={
  {0x315a34,{0x53,0x56,0x57,0x55,0x83}}, {0x1a9d88,{0x53,0x56,0x57,0x83,0xc4}},
  {0x1a4cd4,{0x53,0x56,0x57,0x51,0x8b}}, {0x2cc900,{0x53,0x56,0x83,0xc4,0xe4}},
  {0x1a0834,{0x53,0x56,0x57,0x83,0xc4}}, {ORDER_JUMP,{0x53,0x56,0x57,0x83,0xc4}},
  {ORDER_MOVE,{0x53,0x56,0x57,0x83,0xc4}}, {REPAIR,{0x53,0x56,0x57,0x55,0x8b}},
  {PLANET_RELATION,{0x53,0x56,0x57,0x55,0x83}}, {SET_MONEY,{0x53,0x56,0x8b,0xf2,0x8b}},
  {ATTACKING,{0x53,0x56,0x8b,0xf2,0x8b}}, {JOINT_ATTACK,{0x53,0x56,0x57,0x55,0x83}}
};
static uint8_t installed[sizeof(sites)/sizeof(sites[0])][5];
static int matches(const uint8_t *a,const uint8_t *b){
  for(unsigned i=0;i<5;i++)if(a[i]!=b[i])return 0;
  return 1;
}
static int patch(unsigned i,const uint8_t *before,const uint8_t *after){
  uint32_t old,unused;
  uint8_t *at=image+sites[i].sig.rva;
  if(!matches(at,before)||!VirtualProtect(at,5,0x40,&old))return 0;
  if(!matches(at,before)){VirtualProtect(at,5,old,&unused);return 0;}
  for(unsigned j=0;j<5;j++)at[j]=after[j];
  FlushInstructionCache(GetCurrentProcess(),at,5);
  VirtualProtect(at,5,old,&unused);
  return 1;
}
int32_t FHDNPC_ArmDuties(void){
  if(armed)return 1;
  image=GetModuleHandleW(0);
  if(!image)return 0;
  for(unsigned i=0;i<sizeof(routines)/sizeof(routines[0]);i++)
    if(!matches(image+routines[i].rva,routines[i].bytes))return 0;
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++)
    if(!matches(image+sites[i].sig.rva,sites[i].sig.bytes))return 0;
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++){
    uint32_t delta=(uint32_t)((uintptr_t)sites[i].replacement-(uintptr_t)(image+sites[i].sig.rva+5u));
    installed[i][0]=0xe8;
    for(unsigned j=0;j<4;j++)installed[i][j+1]=(uint8_t)(delta>>(8*j));
    if(!patch(i,sites[i].sig.bytes,installed[i])){
      while(i){i--;patch(i,installed[i],sites[i].sig.bytes);}
      return 0;
    }
  }
  armed=1;return 1;
}
void FHDNPC_DisarmDuties(void){
  if(!armed)return;
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++)patch(i,installed[i],sites[i].sig.bytes);
  armed=0;
}
void FHDNPC_ResetDutyStats(void){for(unsigned i=0;i<12;i++)stats[i]=0;}
void FHDNPC_DutyStats(uint32_t *out){for(unsigned i=0;i<12;i++)out[i]=stats[i];}
void FHDNPC_WorldStats(uint32_t *out){
  if(armed)FHD_WorldStats(day(),out);
  else for(unsigned i=0;i<4;i++)out[i]=0;
}
