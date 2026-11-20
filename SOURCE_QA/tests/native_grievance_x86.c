/* Exercise the same capture/commit code as anchored by the exact game signatures.
   The fabricated lists model what the engine did during pickup and item fire. */
#define FHD_GRIEVANCE_TEST
#include "../src/fhd_grievance.c"

static uint8_t game[0x420000], galaxy[0x1e0], star[0xd4], foreign_star[0xd4];
static uint8_t home_planet[0x170], local_planet[0x170];
static uint8_t pilot[0x530], pirate[0x530], flagship[0x530], trader[0x530], bystander[0x530], partner_ship[0x530], diplomat[0x530], lone_ship[0x530], missile[0x74], item[0x38];
static uint32_t items[3], ships[3], targets[3], parent[3], contents[1], pilots[8], planets_list[3], planet_ptrs[2];
static const uint16_t captain[]={0x041b,0x0430,0x0440,0x0441,0};
static void *g_last_planet;
static void *g_last_ranger;
static int32_t g_last_planet_delta;
static int32_t g_home_delta_sum;
static int32_t g_local_delta_sum;
extern const uint8_t *fhd_test_player_star;
extern unsigned fhd_test_dialogue_calls;

void *GetModuleHandleW(const uint16_t *name){(void)name;return game;}
void *GetCurrentProcess(void){return 0;}
uint16_t *SysAllocStringLen(const uint16_t *s,uint32_t n){(void)s;(void)n;return 0;}
void SysFreeString(uint16_t *s){(void)s;}
int VirtualProtect(void *p,uint32_t n,uint32_t f,uint32_t *old){
  (void)p;(void)n;(void)f;*old=0x40;return 1;
}
int FlushInstructionCache(void *p,const void *a,uint32_t n){(void)p;(void)a;(void)n;return 1;}
void FHDNPC_StationReset(void){}
void FHDNPC_PlayerReactStub(void){}
void FHDNPC_ReputationStub(void){}
void FHDNPC_PickupStub(void){}
void FHDNPC_ShotStub(void){}
void FHDNPC_KillStub(void){}
void FHDNPC_ReputationHomeStub(void){}
void FHDNPC_MissileLootStub(void){}
void FHDNPC_RelationLevelStub(void){}
void FHDNPC_RelationValueStub(void){}
void FHDNPC_RelationRawTrampoline(void){}
void FHDNPC_FormInfoStub(void){}
uint32_t FHDNPC_InvokeExtort(void *f,void *requester,void *target){(void)f;(void)requester;(void)target;return 0;}
uint32_t FHDNPC_InvokeMoneyDemand(void *f,void *target,void *issuer,uint16_t **resp,uint32_t amount){
  (void)f;(void)target;(void)issuer;(void)amount;if(resp)*resp=0;return 1u;
}
void FHDNPC_InvokeJoint(void *f,void *ship,void *other){
  (void)f;
  if(ship==other && ship && other){
    uint32_t v=(uint32_t)(uintptr_t)other;
    for(unsigned i=0;i<4;i++)((uint8_t*)ship)[0x3f4+i]=(uint8_t)(v>>(i*8));
  }
}
void FHDNPC_InvokeSetMoney(void *ship,uint32_t money){(void)ship;(void)money;}
void FHDNPC_InvokePlanetRelation(void *planet,void *ranger,int32_t delta){
  g_last_planet=planet;
  g_last_ranger=ranger;
  g_last_planet_delta=delta;
  if(planet==(void*)home_planet)g_home_delta_sum+=delta;
  if(planet==(void*)local_planet)g_local_delta_sum+=delta;
}
static uint8_t __attribute__((regparm(2))) native_relation(const void *observer,const void *peer){
  (void)observer;(void)peer;return 95u;
}
static uint8_t __attribute__((regparm(2))) can_contact_ok(const void *issuer,const void *target){
  (void)issuer;(void)target;return 1u;
}
static void put(uint8_t *p,unsigned off,uint32_t value){
  for(unsigned i=0;i<4;i++)p[off+i]=(uint8_t)(value>>(i*8));
}
static void ptr(uint8_t *p,unsigned off,const void *value){put(p,off,(uintptr_t)value);}
static void __attribute__((regparm(3))) approach(const uint8_t *ship,const uint32_t *from,uint32_t *dest){
  (void)ship;
  dest[0]=from[0]+1;
  dest[1]=from[1]+2;
}
static int __attribute__((regparm(3))) radar(const uint8_t *ship){(void)ship;return 500;}
static void layout_ship(uint8_t *p,unsigned type,unsigned id,unsigned rva){
  ptr(p,0,game+rva);put(p,4,id);p[0x10]=(uint8_t)type;
  ptr(p,0x24,star);put(p,0xe0,id+123);ptr(p,0x3d8,targets);
}
#define CHECK(expr) do {if(!(expr))return __LINE__;} while(0)
static int run(void){
  g_base=(uintptr_t)game;g_armed=1;fhd_relation_value=(void*)native_relation;
  fhd_planet_relation=(void*)(game+PLANET_CHANGE_REL_RVA);
  game[APPROACH_RVA]=0xe9;
  put(game,APPROACH_RVA+1,(uintptr_t)approach-(uintptr_t)(game+APPROACH_RVA+5));
  fhd_test_player_star=star;
  game[0x2d7288]=0xe9;
  put(game,0x2d7289,(uintptr_t)radar-(uintptr_t)(game+0x2d728d));
  game[CAN_CONTACT_RVA]=0xe9;
  put(game,CAN_CONTACT_RVA+1,(uintptr_t)can_contact_ok-(uintptr_t)(game+CAN_CONTACT_RVA+5));
  ptr(game,0x41d2a0,galaxy);put(galaxy,0x4c,100);
  put(star,4,3);ptr(star,0x2c,ships);ptr(star,0x30,items);
  put(foreign_star,4,9);
  contents[0]=(uintptr_t)item;
  items[1]=targets[1]=(uintptr_t)contents;
  items[2]=targets[2]=1;
  pilots[0]=(uintptr_t)pilot;pilots[1]=(uintptr_t)pirate;pilots[2]=(uintptr_t)flagship;
  ships[1]=(uintptr_t)pilots;ships[2]=3;
  /* Player is TPlayer(TRanger) and intentionally NOT TRanger's VMT. */
  layout_ship(pilot,1,20,PLAYER_VMT_PTR+0x100u);
  put(game,ENCODED_PLAYER_RVA,(uint32_t)(uintptr_t)pilot^PLAYER_XOR_KEY);
  CHECK(player(pilot)&&!player(pirate)&&!ordinary(pilot));
  layout_ship(pirate,3,21,0x1992b0);
  layout_ship(flagship,4,22,0x1a2b10);flagship[0x510]=1;
  pirate[0x11]=7;pirate[0x408]=1;ptr(pirate,8,captain);
  put(item,8,901);put(item,0x20,6500);
  FHDLootFrame event;
  FHDNPC_DialogueModeSet(0);
  FHDNPC_CaptureLootShot(pilot,item,&event);
  CHECK(event.valid&&event.count==1&&event.witness[0].id==21);
  FHDNPC_CompleteLoot(&event);
  CHECK(!FHD_RelationPeer(21,20,143));
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  FHDNPC_CompleteLoot(&event);
  CHECK(FHD_RelationPeer(21,20,143)==-16 && FHDNPC_DialogueQueued()==0);
  CHECK(g_stat[5]==1&&!FHD_ProfileOfIdentity(22,145,0));

  FHDNPC_DialogueModeSet(1);
  put(item,8,902);items[2]=1;
  parent[0]=(uintptr_t)star;parent[1]=(uintptr_t)pilot;parent[2]=(uintptr_t)item;
  FHDNPC_CaptureLootPickup(parent+3,&event);
  CHECK(event.valid&&event.count==1&&event.kind==1);
  items[2]=targets[2]=0;
  FHDNPC_CompleteLoot(&event);
  CHECK(g_stat[4]==1&&FHD_RelationPeer(21,20,143)==-36);
  uint16_t line[256];
  CHECK(FHDNPC_DialoguePopW(line,256)>12&&line[0]=='<'&&line[18]==captain[0]&&line[22]==':');
  CHECK(!FHDNPC_DialogueQueued());

  items[2]=targets[2]=1;
  put(item,8,903);put(pirate,0x14,0x447a0000); /* 1000 units, beyond radar. */
  FHDNPC_CaptureLootShot(pilot,item,&event);
  CHECK(event.valid&&!event.count);
  /* An ordinary competitor depresses the same relation table silently even
     when local radio is enabled and the player is in the same system. */
  put(pirate,0x14,0);
  layout_ship(trader,2,23,0x1ab58c);
  ptr(trader,0x3d8,0);
  pilots[3]=(uintptr_t)trader;ships[2]=4;
  FHDNPC_DialogueModeSet(2);
  unsigned previous_calls=fhd_test_dialogue_calls;
  put(item,8,904);
  FHDNPC_CaptureLootShot(trader,item,&event);
  CHECK(event.valid&&event.count==1);
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  CHECK(FHD_RelationPeer(21,23,146)==-16);
  put(item,8,905);items[2]=1;
  parent[1]=(uintptr_t)trader;
  FHDNPC_CaptureLootPickup(parent+3,&event);
  CHECK(event.valid&&event.count==1);
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  CHECK(FHD_RelationPeer(21,23,146)==-36);
  CHECK(g_stat[4]==2&&g_stat[5]==2&&fhd_test_dialogue_calls==previous_calls);
  CHECK(!FHDNPC_DialogueQueued());
  CHECK(FHDNPC_RelationLevelValue(pirate,pilot,50)==14);
  CHECK(FHDNPC_RelationLevelValue(pirate,trader,50)==14);
  CHECK(FHDNPC_RelationLevelValue(pirate,pilot,6)==6);
  CHECK(FHDNPC_RelationLevelValue(flagship,pilot,50)==50);
  galaxy[0x1d8]=1;
  CHECK(FHDNPC_RelationLevelValue(pirate,pilot,50)==50);
  galaxy[0x1d8]=0;
  put(item,8,906);item[0x0c]=4;put(item,0x20,80);
  items[2]=1;targets[2]=0;
  pirate[0x428]=1;put(pirate,0x434,1);put(pirate,0x438,2);
  ptr(missile,0x3c,pilot);
  FHDNPC_MissileLoot(star,item,missile);
  FHDNPC_MissileLoot(star,item,missile);
  CHECK(g_stat[16]==2&&g_stat[19]==1&&FHD_RelationPeer(21,20,143)==-66);
  CHECK(FHDNPC_RelationLevelValue(pirate,pilot,50)==0);
  CHECK(FHDNPC_DialogueQueued()==1);
  (void)FHDNPC_DialoguePopW(line,256);

  /* Verify 2.2.0 tractor-dragged player pickup detection and immediate claim execution
     even when EXTORT_COOLDOWN_RVA is 0. */
  put(item,8,907);put(item,0x20,450);put(item,0x1c,5);
  /* Item was dragged to (120.0f, 80.0f) while pirate OrderPos was (200.0f, 100.0f). */
  put(item,0x10,0x42f00000u);put(item,0x14,0x42a00000u);
  put(pirate,0x110,1u);put(pirate,0xdc,30u);
  pirate[0x428]=1;put(pirate,0x434,0x43480000u);put(pirate,0x438,0x42c80000u);
  items[2]=1;targets[2]=0;
  parent[0]=(uintptr_t)star;parent[1]=(uintptr_t)pilot;parent[2]=(uintptr_t)item;
  FHDNPC_CaptureLootPickup(parent+3,&event);
  CHECK(event.valid&&event.count==1&&event.witness[0].id==21);
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  CHECK(g_stat[15]==2); /* player_taken incremented! */
  put(game,EXTORT_COOLDOWN_RVA,0u);
  game[DIALOGUE_COUNT_RVA]=0u;
  FHDNPC_GrievanceTick(pirate,2u);
  CHECK(g_claim_results[0]==1&&g_claim_results[1]==1);

  /* Verify 2.2.2 realistic consequences:
     1) Witnessed private trader (id=23) attack & kill:
        - Bonded partner (id=25) reacts and inherits grudge.
        - Unrelated same-race pilot (id=24) out of radar range (1000.0f) is unaffected (0).
        - Victim's HomePlanet becomes Hostile (-100), while local_planet of the same race
          in the same star system is NOT penalized (0) for a private merchant fight! */
  layout_ship(bystander,1,24,0x310c70);
  put(bystander,0x14,0x447a0000u); /* 1000.0f units, out of radar range */
  layout_ship(partner_ship,1,25,0x310c70);
  trader[0x11]=1;bystander[0x11]=1;partner_ship[0x11]=1;
  ptr(trader,0x3fc,partner_ship);ptr(partner_ship,0x3fc,trader);
  home_planet[0x74]=1;home_planet[0x75]=1;ptr(home_planet,0x18,foreign_star);
  local_planet[0x74]=1;local_planet[0x75]=1;ptr(local_planet,0x18,star);
  planet_ptrs[0]=(uintptr_t)home_planet;planet_ptrs[1]=(uintptr_t)local_planet;
  planets_list[1]=(uintptr_t)planet_ptrs;planets_list[2]=2;
  ptr(star,0x24,planets_list);
  ptr(trader,0x2c,home_planet);
  pilots[4]=(uintptr_t)bystander;pilots[5]=(uintptr_t)partner_ship;ships[2]=6;
  g_home_delta_sum=0;g_local_delta_sum=0;
  FHDNPC_GrievanceOnAttack(trader,pilot);
  CHECK(FHDNPC_ShouldSuppressReputation(pilot,trader)==1u);
  CHECK(FHDNPC_IsDefenseVerified(pilot,trader)==0u);
  CHECK(FHD_RelationPeer(24,20,143)==0); /* unrelated same-race pilot unaffected! */
  CHECK(FHD_RelationPeer(25,20,143)<0);  /* bonded partner in same system reacts! */
  int32_t partner_rel_after_attack=FHD_RelationPeer(25,20,143);
  FHDNPC_OnNativeKill(pilot,trader);
  CHECK(g_home_delta_sum==-100 && g_local_delta_sum==-15);
  CHECK(FHD_RelationPeer(24,20,143)==0);
  CHECK(FHD_RelationPeer(25,20,143)<partner_rel_after_attack);
  CHECK(FHDNPC_ShouldSuppressReputation(pilot,trader)==1u);
  CHECK(FHDNPC_IsDefenseVerified(pilot,trader)==0u);

  /* 2) Diplomat (id=26, SubType=2) kill:
        - HomePlanet becomes Hostile (-15 on attack, -100 on kill).
        - Local Coalition planet in the current star system also takes -20 jurisdiction penalty! */
  layout_ship(diplomat,2,26,0x1ab58c);
  diplomat[0x11]=1;diplomat[0x510]=2; /* Diplomat */
  ptr(diplomat,0x2c,home_planet);
  pilots[3]=(uintptr_t)diplomat;
  g_home_delta_sum=0;g_local_delta_sum=0;
  FHDNPC_GrievanceOnAttack(diplomat,pilot);
  CHECK(g_home_delta_sum==-15 && g_local_delta_sum==0);
  FHDNPC_OnNativeKill(pilot,diplomat);
  CHECK(g_home_delta_sum==-115 && g_local_delta_sum==-30);

  /* 3) Outlaw / Pirate (id=21, type=3) with Coalition HomePlanet:
        - Coalition HomePlanet does NOT penalize player for attacking or killing a pirate! */
  ptr(pirate,0x2c,home_planet);
  g_home_delta_sum=0;g_local_delta_sum=0;
  FHDNPC_GrievanceOnAttack(pirate,pilot);
  CHECK(FHDNPC_IsDefenseVerified(pilot,pirate)==1u); /* skips 0x317c89 HomePlanet penalty! */
  FHDNPC_OnNativeKill(pilot,pirate);
  CHECK(g_home_delta_sum==0 && g_local_delta_sum==0);
  CHECK(FHDNPC_IsDefenseVerified(pilot,pirate)==1u);

  /* 4) Silent 1-turn kill of a lone private ship (id=27) in deep space with NO witnesses:
        - Even HomePlanet (in foreign_star) does NOT learn who killed the ship! */
  layout_ship(lone_ship,2,27,0x1ab58c);
  lone_ship[0x11]=1;lone_ship[0x510]=0;
  ptr(lone_ship,0x2c,home_planet);
  /* Move all other NPC ships out of radar range (1000.0f) while pilot & lone_ship are at (0,0). */
  put(pirate,0x14,0x447a0000u);put(flagship,0x14,0x447a0000u);
  put(partner_ship,0x14,0x447a0000u);put(diplomat,0x14,0x447a0000u);
  pilots[3]=(uintptr_t)lone_ship;
  g_home_delta_sum=0;g_local_delta_sum=0;
  FHDNPC_GrievanceOnAttack(lone_ship,pilot);
  CHECK(FHDNPC_IsDefenseVerified(pilot,lone_ship)==1u); /* no witness on turn 1 -> 0x317c89 skipped! */
  FHDNPC_OnNativeKill(pilot,lone_ship);
  CHECK(g_home_delta_sum==0 && g_local_delta_sum==0);
  CHECK(FHDNPC_IsDefenseVerified(pilot,lone_ship)==1u);

  /* 5) Verify v2.2.3: CustomShipType (+0x0c != 0) & Script (+0x3cc != 0) mod ships
        (Special Agent, Freelancer, Explorer) react to player pickup/destruction in front of them,
        reflect changed attitude in FHDNPC_RelationLevelValue, and deliver restitution claims! */
  layout_ship(lone_ship,1,28,0x310c70);
  lone_ship[0x11]=1;
  put(lone_ship,0x0c,0x12345678u);  /* CustomShipType (e.g. SpecAgent/Freelancer/Explorer) */
  put(lone_ship,0x3cc,0x87654321u); /* Script pointer */
  lone_ship[0x44c]=1;               /* ScriptOrder active */
  put(lone_ship,0x110,1u);          /* Grabber installed */
  put(lone_ship,0xc4,40u);          /* Verified TShip.FreeSpace (+0xc4) */
  put(lone_ship,0xdc,0u);
  ptr(lone_ship,0x3d8,0);
  put(lone_ship,0x14,0x42c80000u);  /* 100.0f from item at (0,0) */
  put(lone_ship,0x18,0u);
  put(item,8,908);put(item,0x20,1200);put(item,0x1c,10);
  put(item,0x10,0u);put(item,0x14,0u);
  items[2]=1;targets[2]=0;
  pilots[3]=(uintptr_t)lone_ship;
  parent[0]=(uintptr_t)star;parent[1]=(uintptr_t)pilot;parent[2]=(uintptr_t)item;
  FHDNPC_CaptureLootPickup(parent+3,&event);
  CHECK(event.valid&&event.count==1&&event.witness[0].id==28);
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  CHECK(FHD_RelationPeer(28,20,143)<0);
  CHECK(FHDNPC_RelationLevelValue(lone_ship,pilot,50)<50);
  /* First offense with Friendly native_relation=95 -> pilot forgives without demanding money! */
  game[DIALOGUE_COUNT_RVA]=0u;
  uint32_t prev_claims=g_claim_results[0];
  FHDNPC_GrievanceTick(lone_ship,0u);
  CHECK(g_claim_results[0]==prev_claims);
  /* Second offense (repeat offender, prior < 0) -> now pilot demands 2x compensation! */
  put(item,8,909);items[2]=1;
  FHDNPC_CaptureLootPickup(parent+3,&event);
  items[2]=0;
  FHDNPC_CompleteLoot(&event);
  FHDNPC_GrievanceTick(lone_ship,0u);
  CHECK(g_claim_results[0]==prev_claims+1u);
  return 0;
}
void _start(void){
  int result=run();
  __asm__ volatile ("int $0x80"::"a"(1),"b"(result):"memory");
  __builtin_unreachable();
}
