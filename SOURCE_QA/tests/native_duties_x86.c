/* Execute the patched decision call targets under the actual x86 register ABI.
   Mock engine methods model orders, shop entry and balances. */
#define FHD_DUTIES_TEST
#include "../src/fhd_native_duties.c"
#include "fixtures/duties_layout.h"
static uint8_t game[0x420000], galaxy[0x60], sectors[8];
static uint8_t stars_mem[2][0xd8], planets_mem[2][0x168], ships_mem[9][0x590], buf[32768];
static uint8_t *star_a=stars_mem[0],*star_b=stars_mem[1];
static uint8_t *planet_a=planets_mem[0],*planet_b=planets_mem[1];
static uint8_t *ranger=ships_mem[0],*warrior=ships_mem[1],*pirate=ships_mem[2],
               *trader=ships_mem[3],*liner=ships_mem[4],*envoy=ships_mem[5],
               *station=ships_mem[6],*attacker=ships_mem[7],*clan=ships_mem[8];
static uint8_t *stars_items[]={stars_mem[0],stars_mem[1]};
static uint8_t *a_planets[]={planets_mem[0]}, *b_planets[]={planets_mem[1]};
static uint8_t *a_ships[9],*b_ships[9];
static List stars={0,stars_items,2}, planet_list_a={0,a_planets,1}, planet_list_b={0,b_planets,1};
static List ship_list_a={0,a_ships,9},ship_list_b={0,b_ships,9};
static Distance near_a[]={{0,stars_mem[0]},{10,stars_mem[1]}};
static Distance near_b[]={{0,stars_mem[1]},{10,stars_mem[0]}};
static unsigned stock_calls, needs_repair, shots;

void *GetModuleHandleW(const uint16_t *s){(void)s;return game;}
void *GetCurrentProcess(void){return 0;}
int VirtualProtect(void *p,uint32_t n,uint32_t f,uint32_t *out){
  (void)p;(void)n;(void)f;*out=0x40;return 1;
}
int FlushInstructionCache(void *p,const void *a,uint32_t n){(void)p;(void)a;(void)n;return 1;}
static void REG vanilla(void *p){(void)p;stock_calls++;}
static uint8_t REG vanilla_home(void *p){vanilla(p);return 0;}
static uint8_t REG repair(void *p){(void)p;return (uint8_t)needs_repair;}
static uint8_t REG relation(void *p,void *s){(void)p;(void)s;return 75;}
static uint8_t REG attacking(void *a,void *b){return ref(a,ENEMY)==b;}
static void REG jump(void *p,const void *s,unsigned absolute){
  uint8_t *ship=p;ship[ORDER]=3;ship[ABSOLUTE]=(uint8_t)absolute;ptr(p,TARGET,s);
}
static void REG move(void *p,const void *s,unsigned absolute){
  (void)s;uint8_t *ship=p;ship[ORDER]=1;ship[ABSOLUTE]=(uint8_t)absolute;ptr(p,TARGET,0);
}
static void REG money(void *p,int amount){put(p,MONEY,(unsigned)amount);}
static void REG defend(void *p,void *ally,void *enemy){if(p==ally){ptr(p,ENEMY,enemy);shots++;}}
static void branch(unsigned rva,const void *fn){
  game[rva]=0xe9;put(game,rva+1,(uintptr_t)fn-(uintptr_t)(game+rva+5));
}
static void *call_target(unsigned site){return game+site+5+(int32_t)read32(game,site+1);}
static void on_star(uint8_t *p,uint8_t *s){
  unsigned i=id(p)-10;
  a_ships[i]=s==star_a?p:0;b_ships[i]=s==star_b?p:0;
  ptr(p,STAR,s);ptr(p,PLANET,0);ptr(p,TARGET,0);p[ORDER]=0;
}
static void ship(uint8_t *p,unsigned slot,unsigned type,unsigned role,uint8_t *star){
  unsigned v=type>5?6:type;
  ptr(p,0,game+ship_vmt[v]);put(p,ID,10+slot);put(p,SEED,1500);
  p[TYPE]=(uint8_t)type;p[0x12d]=2;put(p,MONEY,8000);put(p,RANGE,30);
  on_star(p,star);
  FHD_Upsert(id(p),seed(p),type==3?0:type==4?10:4,role,type,0,30,30,40);
}
static int run(void){
  for(unsigned i=0;i<sizeof(routines)/sizeof(routines[0]);i++)copy(game+routines[i].rva,routines[i].bytes,5);
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++)copy(game+sites[i].sig.rva,sites[i].sig.bytes,5);
  game[sites[4].sig.rva]^=1;
  CHECK(!FHDNPC_ArmDuties()&&matches(game+sites[0].sig.rva,sites[0].sig.bytes));
  game[sites[4].sig.rva]^=1;
  CHECK(FHDNPC_ArmDuties());
  branch(0x315a34,vanilla);branch(0x1a9d88,vanilla);branch(0x1a4cd4,vanilla_home);
  branch(0x2cc900,vanilla);branch(0x1a0834,vanilla);
  branch(REPAIR,repair);branch(PLANET_RELATION,relation);branch(ORDER_JUMP,jump);
  branch(ORDER_MOVE,move);branch(SET_MONEY,money);branch(ATTACKING,attacking);branch(JOINT_ATTACK,defend);
  RangerIdle scout=call_target(sites[0].sig.rva);
  Idle patrol=call_target(sites[1].sig.rva), raid=call_target(sites[3].sig.rva);
  Idle clan_raid=call_target(sites[4].sig.rva);
  Check patrol_home=call_target(sites[2].sig.rva);

  ptr(game,GALAXY,galaxy);put(galaxy,0x4c,120);ptr(galaxy,0x2c,&stars);
  put(star_a,ID,1);put(star_b,ID,2);put(sectors,ID,3);
  ptr(star_a,0xd0,sectors);ptr(star_b,0xd0,sectors);
  ptr(star_a,0x94,near_a);ptr(star_b,0x94,near_b);
  ptr(star_a,0x24,&planet_list_a);ptr(star_b,0x24,&planet_list_b);
  ptr(star_a,0x2c,&ship_list_a);ptr(star_b,0x2c,&ship_list_b);
  put(planet_a,ID,100);put(planet_b,ID,200);
  put(planet_a,0x68,50000);put(planet_b,0x68,450000);
  ptr(planet_a,0x18,star_a);ptr(planet_b,0x18,star_b);
  planet_b[0x74]=2;star_b[0x41]=35;
  ship(ranger,0,1,FHD_ROLE_RANGER,star_a);
  ship(warrior,1,4,FHD_ROLE_MILITARY,star_a);
  ship(pirate,2,3,FHD_ROLE_PIRATE,star_a);
  ship(trader,3,2,FHD_ROLE_TRADER,star_a);
  ship(liner,4,2,FHD_ROLE_LINER,star_a);liner[0x510]=1;
  ship(envoy,5,2,FHD_ROLE_DIPLOMAT,star_a);envoy[0x510]=2;
  ship(station,6,8,FHD_ROLE_STATION,star_b);station[OWNER]=2;
  ship(attacker,7,3,FHD_ROLE_PIRATE,star_b);
  ship(clan,8,3,FHD_ROLE_PIRATE,star_a);clan[OWNER]=7;clan[0x514]=1;

  FHDDuty d;
  ranger[ABSOLUTE]=1;scout(ranger,0);
  CHECK(ranger[ORDER]==0&&stock_calls==1&&!FHD_GetDuty(id(ranger),&d));
  ranger[ABSOLUTE]=0;needs_repair=1;scout(ranger,0);
  CHECK(stock_calls==2&&!FHD_GetDuty(id(ranger),&d));needs_repair=0;
  scout(ranger,0);
  CHECK(ranger[ORDER]==3&&ref(ranger,TARGET)==star_b&&FHD_GetDuty(id(ranger),&d)&&d.kind==FHD_DUTY_SCOUT);
  int size=FHD_Snapshot(buf,sizeof(buf));
  CHECK(size>0);FHD_Reset();CHECK(FHD_Restore(buf,(uint32_t)size)==9);
  CHECK(FHD_GetDuty(id(ranger),&d)&&d.star==2);
  on_star(ranger,star_b);scout(ranger,0);
  CHECK(!FHD_GetDuty(id(ranger),&d));

  star_b[0x40]=40;
  FHDNPC_DutyAfter(station,5);
  CHECK(FHD_GetDuty(id(station),&d)&&d.kind==FHD_DUTY_DISTRESS);
  warrior[0x510]=1;patrol(warrior);
  CHECK(!FHD_GetDuty(id(warrior),&d));warrior[0x510]=0;
  CHECK(patrol_home(warrior)&&FHD_GetDuty(id(warrior),&d)&&d.kind==FHD_DUTY_PATROL&&d.peer==id(station));
  on_star(warrior,star_b);patrol(warrior);
  CHECK(warrior[ORDER]==1&&FHD_GetDuty(id(warrior),&d)&&d.phase==1);
  warrior[ORDER]=0;patrol(warrior);
  CHECK(!FHD_GetDuty(id(warrior),&d));

  star_b[0x40]=0;
  raid(pirate);clan_raid(clan);
  CHECK(pirate[ORDER]==3&&clan[ORDER]==3&&FHD_GetDuty(id(pirate),&d)&&d.kind==FHD_DUTY_RAID);
  on_star(pirate,star_b);raid(pirate);
  CHECK(FHD_GetDuty(id(pirate),&d)&&d.phase==1);
  raid(pirate);CHECK(pirate[ORDER]==3&&ref(pirate,TARGET)==star_a);

  CHECK(FHDNPC_TransportDutyDestination(liner)==planet_b);
  CHECK(FHDNPC_TransportDutyDestination(envoy)==planet_b);
  on_star(liner,star_b);ptr(liner,PLANET,planet_b);FHDNPC_DutyAfter(liner,1);
  ptr(envoy,0x2c,planet_a);
  put(planet_a,0x78,200);put(planet_a,0x80,20);put(planet_b,0x84,60);
  on_star(envoy,star_b);ptr(envoy,PLANET,planet_b);FHDNPC_DutyAfter(envoy,1);
  CHECK(FHD_WorldAgreement(100,200,0,120)==120);
  CHECK(!FHD_GetDuty(id(liner),&d)&&!FHD_GetDuty(id(envoy),&d));
  CHECK(FHD_RelationPeer(id(envoy),id(station),seed(station))==0);
  FHDNPC_DutyAfter(envoy,1);CHECK(FHD_RelationPeer(id(envoy),id(station),seed(station))==0);

  on_star(warrior,star_a);on_star(trader,star_a);ptr(trader,PLANET,planet_a);
  FHDTradeGoal trade={200,155,1,20,80};
  FHD_SetTradeGoal(id(trader),&trade);
  FHD_WorldAttack(2,120);FHD_WorldAttack(2,120);
  CHECK(FHDNPC_WaitForEscort(trader));
  CHECK(FHD_GetDuty(id(trader),&d)&&d.kind==FHD_DUTY_REQUEST&&d.fee==276);
  CHECK(patrol_home(warrior));
  CHECK(FHD_GetDuty(id(warrior),&d)&&d.kind==FHD_DUTY_ESCORT);
  CHECK(FHD_GetDuty(id(trader),&d)&&d.kind==FHD_DUTY_CLIENT);
  CHECK(!FHDNPC_WaitForEscort(trader)&&read32(trader,MONEY)==7724);
  size=FHD_Snapshot(buf,sizeof(buf));CHECK(size>0);FHD_Reset();
  CHECK(FHD_Restore(buf,(uint32_t)size)==9);
  on_star(trader,star_b);on_star(warrior,star_b);
  ptr(trader,ENEMY,attacker);ptr(attacker,ENEMY,trader);patrol(warrior);
  CHECK(shots==1&&ref(warrior,ENEMY)==attacker);
  ptr(trader,ENEMY,0);ptr(warrior,ENEMY,0);
  ptr(trader,PLANET,planet_b);
  FHDNPC_DutyAfter(trader,1);
  CHECK(read32(trader,MONEY)==7724&&read32(warrior,MONEY)==8000);
  FHDNPC_TradeDeliveryCompleted(trader,planet_b);
  FHDNPC_TradeDeliveryCompleted(trader,planet_b);
  CHECK(read32(trader,MONEY)==7724&&read32(warrior,MONEY)==8276);
  CHECK(!FHD_GetDuty(id(trader),&d)&&!FHD_GetDuty(id(warrior),&d));
  uint32_t out[12];FHDNPC_DutyStats(out);
  CHECK(out[0]&&out[1]&&out[2]&&out[3]&&out[4]&&out[5]&&out[6]&&out[7]==1&&out[9]&&out[10]==1&&out[11]==1);
  on_star(warrior,star_a);on_star(trader,star_a);ptr(trader,PLANET,planet_a);
  FHD_WorldAttack(2,120);
  FHD_SetTradeGoal(id(trader),&trade);
  CHECK(FHDNPC_WaitForEscort(trader)&&patrol_home(warrior));
  CHECK(read32(trader,MONEY)<7724);
  FHDNPC_TradeCancelled(trader);
  FHDNPC_TradeCancelled(trader);
  CHECK(read32(trader,MONEY)==7724);
  CHECK(!FHD_GetDuty(id(trader),&d));
  FHDNPC_DisarmDuties();
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++)
    CHECK(matches(game+sites[i].sig.rva,sites[i].sig.bytes));
  return 0;
}
void _start(void){
  int result=run();
  __asm__ volatile ("int $0x80"::"a"(1),"b"(result):"memory");
  __builtin_unreachable();
}
