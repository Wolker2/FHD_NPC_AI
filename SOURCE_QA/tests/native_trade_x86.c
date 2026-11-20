/* Execute the actual two replacement calls under the x86 Delphi register ABI.
   The host objects and the remaining engine routines are simulated. */
#define FHD_TRADE_TEST
#include "../src/fhd_native_trade.c"
void *FHDNPC_TransportDutyDestination(void *ship){(void)ship;return 0;}
static unsigned delivered;
int FHDNPC_WaitForEscort(void *ship){(void)ship;return 0;}
void FHDNPC_TradeCancelled(void *ship){(void)ship;}
void FHDNPC_TradeDeliveryCompleted(void *ship,void *planet){(void)ship;(void)planet;delivered++;}

static uint8_t game[0x420000], galaxy[0x54], trader[0x514], source[0x168], target[0x168];
static uint8_t star1[0xd4], star2[0xd4], sector[8], snapshot[32768], alternate[0x168];
static void *destinations[]={target}, *sources[]={source}, *systems[]={star1,star2};
static List planets1={0,sources,1}, planets2={0,destinations,1}, stars={0,systems,2};
static Distance nearby1[]={{0,star1},{13,star2}}, nearby2[]={{0,star2},{13,star1}};
static unsigned stock_trades, stock_routes, repair;

static void put(uint8_t *p,unsigned off,uint32_t value){
  for(unsigned i=0;i<4;i++)p[off+i]=(uint8_t)(value>>(8u*i));
}
static void ptr(uint8_t *p,unsigned off,const void *value){put(p,off,(uintptr_t)value);}
static void copy(uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];}

void *GetModuleHandleW(const uint16_t *s){(void)s;return game;}
void *GetCurrentProcess(void){return 0;}
int VirtualProtect(void *p,uint32_t n,uint32_t f,uint32_t *out){
  (void)p;(void)n;(void)f;*out=0x40;return 1;
}
int FlushInstructionCache(void *p,const void *a,uint32_t n){(void)p;(void)a;(void)n;return 1;}
static void REG stock_market(void *s){(void)s;stock_trades++;}
static void *REG stock_route(void *s){(void)s;stock_routes++;return source;}
static uint8_t REG needs_repair(void *s){(void)s;return (uint8_t)repair;}
static uint8_t REG planet_relation(void *p,void *s){(void)p;(void)s;return 75;}
static int REG price(void *s,unsigned g,void *p){
  (void)s;return (int)number(p,0x84+16u*g);
}
static void REG buy(void *s,unsigned g,int q){
  uint8_t *p=pointer(s,0x1c);
  uint32_t cost=number(p,0x80+16u*g)*(unsigned)q;
  put(s,0xf0,number(s,0xf0)-cost);
  put(p,0x78+16u*g,number(p,0x78+16u*g)-(unsigned)q);
  put(s,0x30+16u*g,(unsigned)q);
  put(s,0x34+16u*g,cost);
}
static void REG sell(void *s,unsigned g,int q){
  uint8_t *p=pointer(s,0x1c);
  put(s,0xf0,number(s,0xf0)+(unsigned)(price(s,g,p)*q));
  put(s,0x30+16u*g,number(s,0x30+16u*g)-(unsigned)q);
  put(p,0x78+16u*g,number(p,0x78+16u*g)+(unsigned)q);
}
static void branch(unsigned rva,const void *fn){
  game[rva]=0xe9;
  put(game,rva+1,(uintptr_t)fn-(uintptr_t)(game+rva+5));
}
static void *call_target(unsigned site){
  return game+site+5+(int32_t)number(game,site+1);
}
#define CHECK(expr) do {if(!(expr))return __LINE__;} while(0)
static int run(void){
  for(unsigned i=0;i<sizeof(routines)/sizeof(routines[0]);i++)
    copy(game+routines[i].rva,routines[i].bytes,5);
  for(unsigned i=0;i<3;i++)copy(game+sites[i],original[i],5);
  for(unsigned i=0;i<200;i++)game[GOODS_LEGAL+i]=1;
  game[sites[1]]^=1;
  CHECK(!FHDNPC_ArmTrade()&&matches(game+sites[0],original[0]));
  game[sites[1]]^=1;
  CHECK(FHDNPC_ArmTrade());
  branch(STOCK_TRADE,stock_market);branch(STOCK_ROUTE,stock_route);
  branch(SELL_PRICE,price);branch(BUY_GOODS,buy);branch(SELL_GOODS,sell);
  branch(NEEDS_REPAIR,needs_repair);
  branch(PLANET_RELATION,planet_relation);
  TradeCall trade=call_target(sites[0]);RouteCall travel=call_target(sites[1]);

  ptr(game,GALAXY,galaxy);ptr(galaxy,0x2c,&stars);put(galaxy,0x4c,120);
  ptr(star1,0x24,&planets1);ptr(star2,0x24,&planets2);
  ptr(star1,0x94,nearby1);ptr(star2,0x94,nearby2);
  put(star1,4,10);put(star2,4,20);ptr(star1,0xd0,sector);ptr(star2,0xd0,sector);
  ptr(source,0x18,star1);ptr(target,0x18,star2);
  put(source,4,101);put(target,4,102);
  put(source,0x78,100);put(source,0x80,20);put(target,0x84,55);
  ptr(trader,0,game+TRANSPORT_VMT);ptr(trader,0x1c,source);ptr(trader,0x24,star1);
  put(trader,4,44);put(trader,0xe0,654);put(trader,0xc8,20);
  put(trader,0xf0,4000);put(trader,0xdc,50);
  trader[0x10]=2;
  CHECK(FHD_Upsert(44,654,4,FHD_ROLE_TRANSPORT,2,0,0,100,0)==1);

  observe_market(target,120);
  target[0x76]=1;game[GOODS_LEGAL+5]=0;
  trade(trader);
  CHECK(stock_trades==1&&cargo(trader,0)==0);
  game[GOODS_LEGAL+5]=1;
  trade(trader);
  FHDTradeGoal goal;
  CHECK(FHD_GetTradeGoal(44,&goal)&&goal.destination==102&&goal.quantity==50);
  CHECK(number(trader,0xf0)==3000&&cargo(trader,0)==50&&number(source,0x78)==50);
  trade(trader);
  CHECK(number(trader,0xf0)==3000&&cargo(trader,0)==50);
  int size=FHD_Snapshot(snapshot,sizeof(snapshot));
  CHECK(size>0);
  FHD_Reset();
  CHECK(FHD_Restore(snapshot,(uint32_t)size)==1);
  ptr(trader,0x1c,0);repair=1;
  CHECK(travel(trader)==source&&stock_routes==1);
  repair=0;trader[0x3b4]=1;
  CHECK(travel(trader)==source&&stock_routes==2);
  trader[0x3b4]=0;star2[0x42]=1;
  CHECK(travel(trader)==source&&stock_routes==3);
  star2[0x42]=0;
  CHECK(travel(trader)==target);
  ptr(trader,0x24,star2);
  CHECK(travel(trader)==target);
  ptr(trader,0x1c,target);
  put(galaxy,0x4c,122);
  trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)&&cargo(trader,0)==0);
  CHECK(number(trader,0xf0)==5750&&number(target,0x78)==50);
  FHDProfile profile;
  CHECK(FHD_ProfileOf(44,&profile)&&profile.events==1&&FHD_TradeExperience(44)==1);
  trade(trader);
  CHECK(number(trader,0xf0)==5750&&stock_trades==2);
  CHECK(FHD_ProfileOf(44,&profile)&&profile.events==1&&FHD_TradeExperience(44)==1);
  uint32_t out[5];
  FHDNPC_TradeStats(out);
  CHECK(out[0]==1&&out[1]==2&&out[2]==1&&out[3]==0&&out[4]==2);
  FHD_Reset();
  CHECK(FHD_Upsert(44,654,4,FHD_ROLE_TRANSPORT,2,0,0,100,0)==1);
  observe_market(target,122);
  ptr(trader,0x1c,source);ptr(trader,0x24,star1);
  trade(trader);
  CHECK(FHD_GetTradeGoal(44,&goal)&&goal.destination==102);
  ptr(trader,0x1c,target);ptr(trader,0x24,star2);
  put(target,0x84,10);
  trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)&&FHD_TradeExperience(44)==-1);
  size=FHD_Snapshot(snapshot,sizeof(snapshot));
  CHECK(size>0);
  FHD_Reset();
  CHECK(FHD_Restore(snapshot,(uint32_t)size)==1&&FHD_TradeExperience(44)==-1);
  FHD_Reset();
  CHECK(FHD_Upsert(44,654,4,FHD_ROLE_SMUGGLER,2,0,0,100,0)==1);
  trader[0x510]=1;
  ptr(trader,0x1c,source);ptr(trader,0x24,star1);
  put(source,0x78+16*5,50);put(source,0x80+16*5,20);
  put(target,0x84+16*5,75);observe_market(target,122);game[GOODS_LEGAL+25*5+5]=0;
  trade(trader);
  CHECK(FHD_GetTradeGoal(44,&goal)&&goal.good==5&&goal.destination==102);
  ptr(trader,0x1c,0);CHECK(travel(trader)==target);
  ptr(trader,0x1c,target);ptr(trader,0x24,star2);trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)&&FHD_TradeExperience(44)==1&&cargo(trader,5)==0&&delivered==3);
  /* The market report is historical. A chain of attacks suppresses new sales,
     while a real delivery reduces the remembered risk and replenishes stock. */
  FHD_Reset();
  CHECK(FHD_Upsert(44,654,4,FHD_ROLE_TRANSPORT,2,0,0,100,0)==1);
  trader[0x510]=0;put(source,0x78,100);put(target,0x78,0);put(target,0x84,95);
  ptr(trader,0x1c,source);ptr(trader,0x24,star1);
  trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)); /* unknown market */
  observe_market(target,122);
  for(unsigned i=0;i<5;i++)FHD_WorldAttack(20,122);
  trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)&&FHD_WorldRisk(20,122)==90);
  put(galaxy,0x4c,162);
  trade(trader);
  CHECK(FHD_GetTradeGoal(44,&goal)&&goal.destination==102);
  unsigned quote,need;
  CHECK(FHD_WorldQuote(102,20,162,0,&quote,&need)&&quote==57&&need==28&&goal.quantity==28);
  ptr(trader,0x1c,target);ptr(trader,0x24,star2);
  trade(trader);
  CHECK(FHD_WorldRisk(20,162)==69&&number(target,0x78)>0);
  CHECK(FHD_WorldQuote(102,20,162,0,&quote,&need)&&need<140);
  size=FHD_Snapshot(snapshot,sizeof(snapshot));
  CHECK(size>0);FHD_Reset();
  CHECK(FHD_Restore(snapshot,(uint32_t)size)==1&&FHD_WorldRisk(20,162)==69);
  CHECK(FHD_WorldQuote(102,20,162,0,&quote,&need)&&need<140);
  FHD_Reset();
  CHECK(FHD_Upsert(44,654,4,FHD_ROLE_TRANSPORT,2,0,0,100,0)==1);
  CHECK(FHD_Upsert(45,655,4,FHD_ROLE_TRANSPORT,2,0,0,100,0)==1);
  put(source,0x78,100);put(target,0x78,110);put(target,0x84,95);
  observe_market(target,162);
  FHDTradeGoal competing={102,190,0,50,20};
  FHD_SetTradeGoal(45,&competing);
  ptr(trader,0x1c,source);ptr(trader,0x24,star1);
  trade(trader);
  CHECK(!FHD_GetTradeGoal(44,&goal)&&cargo(trader,0)==0);
  FHD_SetTradeGoal(45,0);
  trade(trader);
  CHECK(FHD_GetTradeGoal(44,&goal)&&goal.quantity==30);
  FHD_SetTradeGoal(45,&competing);
  uint8_t *two_markets[]={target,alternate};
  planets2.items=(void **)two_markets;planets2.count=2;
  ptr(alternate,0x18,star2);put(alternate,4,103);put(alternate,0x84,70);
  observe_market(alternate,162);
  CHECK(FHD_WorldAgree(101,103,0,30,162));
  ptr(trader,0x1c,0);
  CHECK(travel(trader)==alternate&&FHD_TradeOrigin(44)==101);
  ptr(trader,0x1c,alternate);ptr(trader,0x24,star2);
  trade(trader);
  CHECK(!FHD_WorldAgreement(101,103,0,162)&&number(alternate,0x78)==30);
  FHDNPC_DisarmTrade();
  CHECK(matches(game+sites[0],original[0])&&matches(game+sites[1],original[1])&&matches(game+sites[2],original[2]));
  return 0;
}

void _start(void){
  int result=run();
  __asm__ volatile ("int $0x80"::"a"(1),"b"(result):"memory");
  __builtin_unreachable();
}
