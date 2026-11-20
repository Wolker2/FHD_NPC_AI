/* The stock transport daily routine owns maintenance, movement and combat.
   Only its trading and discretionary destination call sites dispatch here. */
#include <stdint.h>
#include "fhd_npc_ai.h"
#include "fhd_native_duties.h"
#include "fhd_world.h"
#include "fhd_dialogue.h"
#if defined(FHD_TRADE_TEST) && defined(__i386__)
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
  TRANSPORT_VMT=0x1ab58c, GALAXY=0x41d2a0,
  STOCK_TRADE=0x1acae4, STOCK_ROUTE=0x1ac84c, STOCK_TAKEOFF=0x2e2968,
  SELL_PRICE=0x2d52ac, BUY_GOODS=0x2d5bfc, SELL_GOODS=0x2d5638,
  PLANET_RELATION=0x339578,
  NEEDS_REPAIR=0x2c9a68, GOODS_LEGAL=0x411918
};
typedef void (REG *GoodsCall)(void *,unsigned,int);
typedef int (REG *PriceCall)(void *,unsigned,void *);
typedef void (REG *TradeCall)(void *);
typedef void *(REG *RouteCall)(void *);
typedef uint8_t (REG *RepairCheck)(void *);
typedef uint8_t (REG *PlanetRelation)(void *,void *);

typedef struct {void *vmt;void **items;int count;} List;
typedef struct {int distance;uint8_t *star;} Distance;
typedef struct {int cost,quantity;} Lot;
enum {MAX_NEARBY_STARS=12, MAX_PLANETS_PER_STAR=12, BASIC_GOODS=5, MAX_LOT=200};
static uint8_t *image;
static unsigned armed;
static uint32_t stats[5]; /* purchase, routed, sold, cancelled, native fallback */
static const uint32_t sites[]={0x1ac35e,0x1ac60a,0x1ac407};
static const uint8_t original[3][5]={{0xe8,0x81,0x07,0,0},{0xe8,0x3d,0x02,0,0},{0xe8,0x5c,0x65,0x13,0}};
static const struct {uint32_t rva;uint8_t bytes[5];} routines[]={
  {STOCK_TRADE,{0x53,0x56,0x57,0x83,0xc4}},
  {STOCK_TAKEOFF,{0x53,0x56,0x57,0x55,0x83}},
  {STOCK_ROUTE,{0x53,0x8b,0xd8,0x8b,0xc3}},
  {SELL_PRICE,{0x55,0x8b,0xec,0x81,0xc4}},
  {BUY_GOODS,{0x53,0x56,0x57,0x55,0x83}},
  {SELL_GOODS,{0x53,0x56,0x57,0x55,0x83}},
  {NEEDS_REPAIR,{0x53,0x56,0x57,0x55,0x8b}},
  {PLANET_RELATION,{0x53,0x56,0x57,0x55,0x83}}
};
static uint8_t installed[3][5];

static uint32_t number(const uint8_t *p,unsigned offset){
  p+=offset;
  return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void *pointer(const uint8_t *p,unsigned offset){return (void *)(uintptr_t)number(p,offset);}
static uint32_t day(void){
  const uint8_t *world=pointer(image,GALAXY);
  return world?number(world,0x4c):0;
}
static int eligible(const uint8_t *p){
  return p && pointer(p,0)==image+TRANSPORT_VMT && p[0x10]==2u && p[0x510]<=2u &&
    p[0x11]<=4u && !number(p,0x0c) && !number(p,0x3cc) &&
    !p[0x43c] && !p[0x44c] && !p[0x458] && !p[0x459] &&
    !p[0x460] && !p[0x461] && !p[0x49c];
}
static int free_to_trade(const uint8_t *p){
  return !number(p,0x3d0)&&!number(p,0x3fc)&&!p[0x3b4]&&!number(p,0x3f4);
}
static int safe_star(const uint8_t *star){
  const uint8_t *sector=star?pointer(star,0xd0):0;
  return sector&&number(sector,4)!=20u&&star[0x42]==0&&!star[0x48]&&!number(star,0x44);
}
static int safe_planet(const uint8_t *p){
  return p&&p[0x74]<=4u&&!p[0x15c]&&!pointer(p,0x164)&&safe_star(pointer(p,0x18));
}
static void observe_market(const uint8_t *p,uint32_t today){
  uint16_t prices[8],needs[8];
  const uint8_t *star=pointer(p,0x18);
  for(unsigned good=0;good<8;good++){
    unsigned offset=0x78+16u*good;
    int stock=(int)number(p,offset),price=(int)number(p,offset+12);
    prices[good]=(uint16_t)(price>0&&price<=65535?price:0);
    int desired=good==0?140:100;
    needs[good]=(uint16_t)(stock<0?0:stock>=desired?0:desired-stock);
  }
  FHD_WorldObserve(number(p,4),number(star,4),today,prices,needs);
}
static int may_land(void *ship,void *planet){
  return ((PlanetRelation)(image+PLANET_RELATION))(planet,ship)>=25;
}
static int legal(const uint8_t *planet,unsigned good){
  unsigned race=planet[0x76],government=planet[0x77];
  if(good>=8||race>=5||government>=5)return -1;
  return !!image[GOODS_LEGAL+(good*5u+race)*5u+government];
}
static int bounded(int value,int limit){return value<limit?value:limit;}
static int cargo(const uint8_t *ship,unsigned good){return (int)number(ship,0x30+16u*good);}
static int sale_price(void *ship,unsigned good,void *planet){return ((PriceCall)(image+SELL_PRICE))(ship,good,planet);}
static void fallback_trade(void *ship){stats[4]++;((TradeCall)(image+STOCK_TRADE))(ship);}

/* Autonomous transport salvage through the ORIGINAL item selector, which
   verifies value, hook capability, cargo room and competitors. Never create
   an item or mutate a cargo slot. Do not interrupt a live trade contract or
   quest/land/jump/combat order. Only scout in controlled, low-risk space. */
static uint32_t salvage_last_ship;
static uint32_t salvage_last_day;
static uint32_t salvage_attempted, salvage_ordered;
static uint32_t salvage_reasons[9]; /* eligible,not_in_space,no_hook,existing_order,unsafe_star,route,profile,throttled,attempted */
void FHDNPC_TransportSalvageDiagnostics(uint32_t*out){
 if(out)for(uint32_t k=0;k<9u;k++)out[k]=salvage_reasons[k];
}
void FHDNPC_TransportSalvageStats(uint32_t*out){
 if(out){out[0]=salvage_attempted;out[1]=salvage_ordered;}
}
static int has_active_salvage(const uint8_t *ship){
 const List *targets=pointer(ship,0x3d8u);
 return targets && targets->items && targets->count>0;
}
void FHDNPC_TransportSalvage(void *object){
 uint8_t *ship=(uint8_t*)object;
 uint32_t today=day(),id;
 FHDProfile profile;
 FHDTradeGoal goal;
 FHDDuty duty;
 if(!armed||!image||!eligible(ship)||!today)return;
 salvage_reasons[0]++;
 if(!pointer(ship,0x24)||pointer(ship,0x1c)||pointer(ship,0x20)||number(ship,0xdc)<20u){
    salvage_reasons[1]++;return;
 }
 if(!pointer(ship,0x110u)){salvage_reasons[2]++;return;}
 id=number(ship,4u);
 if(FHD_GetTradeGoal(id,&goal)){salvage_reasons[5]++;return;}
 /* TTransport.NextDayLogic (0x5AC5F0) assigns a default move (Order=1) or
    discretionary planet landing (Order=2) before FHDNPC_AfterShipTick runs.
    Allow salvage evaluation when the transport has no active trade goal,
    no duty, no active pickup targets, and only a routine order <= 2. */
 if(!free_to_trade(ship)||ship[0x428]>2u||ship[0x43cu]||has_active_salvage(ship)||
    FHD_GetDuty(id,&duty)){
    salvage_reasons[3]++;return;
 }
 if(!safe_star(pointer(ship,0x24))||
    FHD_WorldRisk(number(pointer(ship,0x24),4u),today)>40u){
    salvage_reasons[4]++;return;
 }
 if(!FHD_ProfileOfIdentity(id,number(ship,0xe0u),&profile)||
    (profile.role!=FHD_ROLE_TRADER&&profile.role!=FHD_ROLE_TRANSPORT&&
     profile.role!=FHD_ROLE_LINER)){salvage_reasons[6]++;return;}
 if(salvage_last_ship==id && salvage_last_day==today){salvage_reasons[7]++;return;}
 salvage_last_ship=id;salvage_last_day=today;
 typedef uint8_t (REG *TryCollect)(void*,int);
 salvage_attempted++;salvage_reasons[8]++;
 /* Temporarily enable TTransport VMT+0x94 (CanCollectItemClass) to point to
    TRanger's CanCollectItemClass (0x31e334: mov al, 1; ret) and save/restore
    Order (0x428) so TryCollectItems does not clear an existing move order
    when no item is selected. */
 uint8_t prev_order=ship[0x428u];
 void **vmt_can_collect=(void**)(image+TRANSPORT_VMT+0x94u);
 uint32_t old_prot=0,unused_prot=0;
 int vmt_patched=0;
 void *orig_can_collect=0;
 if(VirtualProtect(vmt_can_collect,4u,0x40u,&old_prot)){
   orig_can_collect=*vmt_can_collect;
   if(orig_can_collect==(void*)(image+0x2e1c8cu)){
     *vmt_can_collect=(void*)(image+0x31e334u);
     vmt_patched=1;
   }
 }
 ship[0x428u]=0u;
 uint8_t collected=((TryCollect)(image+0x2e1860u))(object,3);
 if(vmt_patched){
   *vmt_can_collect=orig_can_collect;
   VirtualProtect(vmt_can_collect,4u,old_prot,&unused_prot);
 }else if(old_prot){
   VirtualProtect(vmt_can_collect,4u,old_prot,&unused_prot);
 }
 /* A real move/pickup order, not merely a tempting item in the star. */
 if((ship[0x428u]==1u && pointer(ship,0x3d8u)) || has_active_salvage(ship) || collected){
   salvage_ordered++;
 }else if(ship[0x428u]==0u){
   ship[0x428u]=prev_order;
 }
}


static void REG market(void *object){
  uint8_t *ship=object,*planet=pointer(ship,0x1c);
  FHDProfile person;
  FHDTradeGoal goal;
  uint32_t id=number(ship,4),today=day();
  if(!eligible(ship)||!today||!FHD_ProfileOfIdentity(id,number(ship,0xe0),&person)){
    fallback_trade(ship);return;
  }
  if(safe_planet(planet))observe_market(planet,today);
  if(ship[0x510]&&person.role!=FHD_ROLE_SMUGGLER){
    fallback_trade(ship);return;
  }
  if(!free_to_trade(ship)){
    /* Preserve the purchased lot if temporary danger or an order delays the plan. */
    if(!FHD_GetTradeGoal(id,&goal))fallback_trade(ship);
    return;
  }
  if(FHD_GetTradeGoal(id,&goal)){
    int quantity=cargo(ship,goal.good);
    if(today>goal.expires||quantity<(int)goal.quantity){
      FHD_SetTradeGoal(id,0);FHDNPC_TradeCancelled(ship);stats[3]++;
    }else if(planet&&number(planet,4)==goal.destination&&safe_planet(planet)&&
             (person.role==FHD_ROLE_SMUGGLER||legal(planet,goal.good)==1)){
      int price=sale_price(ship,goal.good,planet);
      ((GoodsCall)(image+SELL_GOODS))(ship,goal.good,(int)goal.quantity);
      if(cargo(ship,goal.good)==quantity-(int)goal.quantity){
        if(price!=(int)goal.unit_cost)
          FHD_Record(id,0,price>(int)goal.unit_cost?FHD_EVENT_PROFIT:FHD_EVENT_LOSS,1,today);
        else FHD_Record(id,0,FHD_EVENT_TRADE,1,today);
        FHDNPC_TradeDeliveryCompleted(ship,planet);
        FHD_WorldDelivery(number(pointer(planet,0x18),4),today,goal.quantity);
        unsigned fulfilled=FHD_WorldFulfil(FHD_TradeOrigin(id),goal.destination,goal.good,goal.quantity,today);
        if(fulfilled==2)
          FHDNPC_LocalDialogue(FHD_DLG_DELIVERY,ship,goal.destination,0,today,goal.good);
        observe_market(planet,today);
        FHD_SetTradeGoal(id,0);stats[2]++;
      }
      return; /* No second sale or unrelated native trade in this invocation. */
    }else return; /* A stopover does not consume the planned cargo. */
  }
  if(!safe_planet(planet)){fallback_trade(ship);return;}
  const uint8_t *star=pointer(planet,0x18);
  const Distance *nearby=pointer(star,0x94);
  const uint8_t *world=pointer(image,GALAXY);
  const List *stars=pointer(world,0x2c);
  int money=(int)number(ship,0xf0),free=(int)number(ship,0xdc);
  int range=(int)number(ship,0xc8);
  int experience=FHD_TradeExperience(id);
  int reserve=money/4;
  if(reserve<500)reserve=500;
  if(!nearby||!stars||!stars->items||money<=reserve||free<=0||range<0){
    fallback_trade(ship);return;
  }
  Lot available[8];
  unsigned lots=0;
  unsigned first=person.role==FHD_ROLE_SMUGGLER?BASIC_GOODS:0u;
  unsigned last=first?8u:BASIC_GOODS;
  for(unsigned good=first;good<last;good++){
    int stock=(int)number(planet,0x78+16u*good);
    int cost=(int)number(planet,0x80+16u*good);
    if(stock<=0||cost<1||cost>1000000||cargo(ship,good)||legal(planet,good)!=1)continue;
    int quantity=bounded(bounded(bounded(stock,free),(money-reserve)/cost),MAX_LOT);
    if(quantity>0){available[good]=(Lot){cost,quantity};lots|=1u<<good;}
  }
  if(!lots){fallback_trade(ship);return;}
  int32_t best=0;
  FHDTradeGoal chosen={0};
  for(int i=0;i<bounded(stars->count,MAX_NEARBY_STARS);i++){
    const uint8_t *next=nearby[i].star;
    int distance=nearby[i].distance;
    if(!safe_star(next)||distance<0||distance>range)continue;
    uint32_t star_id=number(next,4);
    unsigned risk=FHD_WorldRisk(star_id,today);
    if(risk>75u)continue;
    const List *planets=pointer(next,0x24);
    if(!planets||!planets->items)continue;
    for(int j=0;j<bounded(planets->count,MAX_PLANETS_PER_STAR);j++){
      uint8_t *target=planets->items[j];
      if(target==planet||!safe_planet(target)||!may_land(ship,target))continue;
      for(unsigned good=first;good<last;good++){
        if(!(lots&(1u<<good))||legal(target,good)!=(first?0:1))continue;
        unsigned quote,need;
        if(!FHD_WorldQuote(number(target,4),star_id,today,good,&quote,&need))continue;
        unsigned incoming=FHD_IncomingCargo(number(target,4),good,id,today);
        unsigned agreement=first?0:FHD_WorldAgreement(number(planet,4),number(target,4),good,today);
        unsigned demand=agreement>need?agreement:need;
        if(incoming&&incoming>=demand)continue;
        need=demand>incoming?demand-incoming:0;
        int cost=available[good].cost,q=available[good].quantity;
        if(need&&q>(int)need)q=(int)need;
        int gain=(int)quote-cost;
        if(gain<=0||gain>1000000)continue;
        int min_profit=30+distance*(3+person.traits[3]/20)+cost/20-experience+
                       (int)risk*(3+person.traits[3]/16);
        if(first)min_profit+=120+(100-person.traits[5]);
        int32_t profit=gain*q;
        if(profit<=min_profit)continue;
        int32_t score=(profit-min_profit+(int)need+(agreement?(int)need/2:0))/(2+distance);
        uint32_t destination=number(target,4);
        if(destination && score>best){
          best=score;chosen=(FHDTradeGoal){destination,today+100u,good,(uint32_t)q,(uint32_t)cost};
        }
      }
    }
  }
  if(!chosen.destination){fallback_trade(ship);return;}
  ((GoodsCall)(image+BUY_GOODS))(ship,chosen.good,(int)chosen.quantity);
  if(cargo(ship,chosen.good)==(int)chosen.quantity){
    int basis=(int)number(ship,0x34+16u*chosen.good);
    if(basis>0)chosen.unit_cost=(uint32_t)basis/chosen.quantity;
    FHD_SetTradeGoal(id,&chosen);stats[0]++;
    FHD_SetTradeOrigin(id,number(planet,4));
  }
}

static void *reachable_destination(uint8_t *ship,const FHDTradeGoal *goal,uint32_t role,uint32_t today){
  const uint8_t *current=pointer(ship,0x24);
  const uint8_t *world=pointer(image,GALAXY);
  const List *stars=world?pointer(world,0x2c):0;
  const Distance *nearby=current?pointer(current,0x94):0;
  int range=(int)number(ship,0xc8);
  if(!stars||!nearby||!safe_star(current))return 0;
  for(int i=0;i<bounded(stars->count,MAX_NEARBY_STARS);i++){
    const uint8_t *star=nearby[i].star;
    if(!safe_star(star)||nearby[i].distance<0||nearby[i].distance>range||
       FHD_WorldRisk(number(star,4),today)>75)continue;
    const List *planets=pointer(star,0x24);
    if(!planets||!planets->items)continue;
    for(int j=0;j<bounded(planets->count,MAX_PLANETS_PER_STAR);j++){
      uint8_t *planet=planets->items[j];
      if(safe_planet(planet)&&number(planet,4)==goal->destination&&may_land(ship,planet)&&
         (role==FHD_ROLE_SMUGGLER||legal(planet,goal->good)==1))
        return planet;
    }
  }
  return 0;
}

static void *alternative_market(uint8_t *ship,const FHDTradeGoal *goal,
                                uint32_t day,uint32_t role){
  const uint8_t *current=pointer(ship,0x24),*world=pointer(image,GALAXY);
  const List *stars=world?pointer(world,0x2c):0;
  const Distance *nearby=current?pointer(current,0x94):0;
  if(!stars||!nearby||!safe_star(current))return 0;
  int best=-1000000,range=(int)number(ship,0xc8);
  void *route=0;
  for(int i=0;i<bounded(stars->count,MAX_NEARBY_STARS);i++){
    const uint8_t *star=nearby[i].star;
    int dist=nearby[i].distance;
    if(!safe_star(star)||dist<0||dist>range)continue;
    uint32_t sid=number(star,4);
    unsigned risk=FHD_WorldRisk(sid,day);
    if(risk>75)continue;
    const List *planets=pointer(star,0x24);
    if(!planets||!planets->items)continue;
    for(int j=0;j<bounded(planets->count,MAX_PLANETS_PER_STAR);j++){
      uint8_t *planet=planets->items[j];
      if(!safe_planet(planet)||number(planet,4)==goal->destination||!may_land(ship,planet))continue;
      unsigned quote,need;
      if(legal(planet,goal->good)!=(role==FHD_ROLE_SMUGGLER?0:1)||
         !FHD_WorldQuote(number(planet,4),sid,day,goal->good,&quote,&need))continue;
      unsigned incoming=FHD_IncomingCargo(number(planet,4),goal->good,number(ship,4),day);
      if(incoming&&incoming>=need)continue;
      need=need>incoming?need-incoming:0;
      int score=(int)quote+(int)need/8-dist*3-(int)risk;
      if(score>best){best=score;route=planet;}
    }
  }
  return route;
}

static void *REG route(void *object){
  uint8_t *ship=object;
  FHDTradeGoal goal;
  FHDProfile profile;
  uint32_t id=number(ship,4),today=day();
  if(eligible(ship)&&free_to_trade(ship)&&today&&FHD_ProfileOfIdentity(id,number(ship,0xe0),&profile)&&
     (!ship[0x510]||profile.role==FHD_ROLE_SMUGGLER)&&FHD_GetTradeGoal(id,&goal)){
    /* Keep a temporarily unreachable cargo plan while vanilla reroutes/refuels. */
    if(today>goal.expires||cargo(ship,goal.good)<(int)goal.quantity){
      FHD_SetTradeGoal(id,0);FHDNPC_TradeCancelled(ship);stats[3]++;
    }else if(!((RepairCheck)(image+NEEDS_REPAIR))(ship)){
      void *planet=reachable_destination(ship,&goal,profile.role,today);
      unsigned competing=FHD_IncomingCargo(goal.destination,goal.good,id,today);
      int crowded=0;
      if(planet&&competing){
        unsigned quote,need;
        uint32_t sid=number(pointer(planet,0x18),4);
        crowded=FHD_WorldQuote(goal.destination,sid,today,goal.good,&quote,&need)&&
                competing>=need&&competing>=goal.quantity;
      }
      if(!planet||crowded){
        void *alternative=alternative_market(ship,&goal,today,profile.role);
        if(alternative){
          planet=alternative;
          FHDNPC_TradeCancelled(ship);
          goal.destination=number(planet,4);
          FHD_SetTradeGoal(id,&goal);
          if(crowded)FHDNPC_LocalDialogue(FHD_DLG_RIVALRY,ship,goal.destination,0,today,goal.good);
        }
      }
      /* The engine uses this result for its existing OrderLanding / OrderJump. */
      if(planet){stats[1]++;return planet;}
    }
  }
  if(ship[0x510]!=0u&&!((RepairCheck)(image+NEEDS_REPAIR))(ship)){
    void *destination=FHDNPC_TransportDutyDestination(ship);
    if(destination)return destination;
  }
  return ((RouteCall)(image+STOCK_ROUTE))(ship);
}

static void REG takeoff(void *object){
  if(!FHDNPC_WaitForEscort(object))((TradeCall)(image+STOCK_TAKEOFF))(object);
}
static int matches(const uint8_t *a,const uint8_t *b){
  for(unsigned j=0;j<5;j++)if(a[j]!=b[j])return 0;
  return 1;
}
static int patch(unsigned i,const uint8_t *before,const uint8_t *after){
  uint32_t old,unused;
  uint8_t *at=image+sites[i];
  if(!matches(at,before)||!VirtualProtect(at,5,0x40,&old))return 0;
  if(!matches(at,before)){VirtualProtect(at,5,old,&unused);return 0;}
  for(unsigned j=0;j<5;j++)at[j]=after[j];
  FlushInstructionCache(GetCurrentProcess(),at,5);
  VirtualProtect(at,5,old,&unused);return 1;
}
int32_t FHDNPC_ArmTrade(void){
  if(armed)return 1;
  image=GetModuleHandleW(0);
  if(!image)return 0;
  for(unsigned i=0;i<3;i++)if(!matches(image+sites[i],original[i]))return 0;
  for(unsigned i=0;i<sizeof(routines)/sizeof(routines[0]);i++)
    if(!matches(image+routines[i].rva,routines[i].bytes))return 0;
  const void *replacement[]={market,route,takeoff};
  for(unsigned i=0;i<3;i++){
    uint32_t delta=(uint32_t)((uintptr_t)replacement[i]-(uintptr_t)(image+sites[i]+5u));
    installed[i][0]=0xe8;
    for(unsigned j=0;j<4;j++)installed[i][j+1]=(uint8_t)(delta>>(j*8));
    if(!patch(i,original[i],installed[i])){
      while(i){i--;patch(i,installed[i],original[i]);}
      return 0;
    }
  }
  armed=1;return 1;
}
void FHDNPC_DisarmTrade(void){
  if(!armed)return;
  for(unsigned i=0;i<3;i++)patch(i,installed[i],original[i]);
  armed=0;
}
void FHDNPC_ResetTradeStats(void){
  for(unsigned i=0;i<5;i++)stats[i]=0;
  salvage_last_ship=0;salvage_last_day=0;
  salvage_attempted=0;salvage_ordered=0;
  for(unsigned i=0;i<9;i++)salvage_reasons[i]=0;
}
void FHDNPC_TradeStats(uint32_t *out){for(unsigned i=0;i<5;i++)out[i]=stats[i];}
