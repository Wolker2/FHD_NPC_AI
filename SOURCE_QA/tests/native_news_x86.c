/* Exercise the actual local-radio adapter on x86 with the engine endpoints simulated. */
#define FHD_NEWS_TEST
#include "../src/fhd_news.c"

static uint8_t game[0x420000], galaxy[0x54], player[0x530], speaker[0x530];
static uint8_t local_star[0x50], remote_star[0x50];
static uint16_t displayed[8][MAX_MSG_LEN], allocated[MAX_MSG_LEN];
static const uint16_t captain[]=u"Ларс",full_name[]=u"корсар Ларс";
static uint32_t vmt[12],lock_object;
static uint8_t bubble[0x44];
static unsigned deliveries, information, fail_allocation,lock_depth,targets_linked;
static void put(uint8_t *p,unsigned off,uint32_t value){
 for(unsigned i=0;i<4;i++)p[off+i]=(uint8_t)(value>>(i*8));
}
static void point(uint8_t *p,unsigned off,const void *value){put(p,off,(uintptr_t)value);}
void *GetModuleHandleW(const uint16_t *name){(void)name;return game;}
int32_t FHDNPC_Status(void){return 1;}
uint16_t *SysAllocStringLen(const uint16_t *src,uint32_t len){
 if(fail_allocation||len>=MAX_MSG_LEN)return 0;
 for(unsigned i=0;i<len;i++)allocated[i]=src[i];
 allocated[len]=0;return allocated;
}
void SysFreeString(uint16_t *p){(void)p;}
void *FHDNPC_InvokeNews(void *entry,uint32_t kind,uint32_t turn,const uint16_t *text){
 if(lock_depth!=1||entry!=game+NEWS_RVA||(kind!=RADIO_KIND&&kind!=INFO_KIND)||
    turn!=number(galaxy,0x4c)||deliveries>=8)return 0;
 if(kind==INFO_KIND){information++;return bubble;}
 for(unsigned i=0;i<MAX_MSG_LEN;i++){
   displayed[deliveries][i]=text[i];
   if(!text[i])break;
 }
 deliveries++;return bubble;
}
static void __attribute__((regparm(3))) full(const uint8_t *ship,const uint16_t *sep,uint16_t **out){
 if(ship==speaker&&sep[0]==' '&&!sep[1])*out=(uint16_t *)full_name;
}
static void __attribute__((regparm(3))) lock_enter(const uint8_t *lock){
 if(lock==(const uint8_t *)&lock_object)lock_depth++;
}
static void __attribute__((regparm(3))) lock_leave(const uint8_t *lock){
 if(lock==(const uint8_t *)&lock_object&&lock_depth){
   if(number(bubble,0x24)==17)targets_linked++;
   put(bubble,0x24,0);
   lock_depth--;
 }
}
static const uint8_t *get_player(void){return player;}
static void copy(uint8_t *p,const uint8_t *from,unsigned n){
 for(unsigned i=0;i<n;i++)p[i]=from[i];
}
static int different(const uint16_t *a,const uint16_t *b){
 while(*a&&*a==*b){a++;b++;}
 return *a!=*b;
}
#define CHECK(expr) do {if(!(expr))return __LINE__;} while(0)
static int run(void){
 const uint8_t news[]={0x55,0x8b,0xec,0x83,0xc4,0xf4,0x53,0x56,0x57,0x8b,0xf9},
               player_sig[]={0xa1,0xfc,0x00,0x81,0x00,0x35,0xd3,0x15,0xcd,0xb1,0xc3};
 copy(game+NEWS_RVA,news,sizeof(news));copy(game+GET_PLAYER_RVA,player_sig,sizeof(player_sig));
 const uint8_t enter[]={0x8b,0x10,0xff,0x12,0xc3},
               leave[]={0x8b,0x10,0xff,0x52,0x04,0xc3};
 copy(game+LOCK_ENTER,enter,sizeof(enter));copy(game+LOCK_LEAVE,leave,sizeof(leave));
 CHECK(verify_entry());
 game[LOCK_ENTER]=0xe9;put(game,LOCK_ENTER+1,(uintptr_t)lock_enter-(uintptr_t)(game+LOCK_ENTER+5));
 game[LOCK_LEAVE]=0xe9;put(game,LOCK_LEAVE+1,(uintptr_t)lock_leave-(uintptr_t)(game+LOCK_LEAVE+5));
 point(game,MESSAGE_LOCK,&lock_object);
 point(speaker,0,vmt);vmt[0x28/4]=(uintptr_t)full;
 game[GET_PLAYER_RVA]=0xe9;
 put(game,GET_PLAYER_RVA+1,(uintptr_t)get_player-(uintptr_t)(game+GET_PLAYER_RVA+5));
 point(game,GALAXY_RVA,galaxy);put(galaxy,0x4c,100);
 put(local_star,4,15);put(remote_star,4,16);
 point(player,0x24,local_star);
 point(speaker,0x24,remote_star);point(speaker,8,captain);put(speaker,4,17);
 speaker[0x408]=1;speaker[0x10]=6; static uint32_t ship_arr[1]; static uint32_t ship_list[3]; ship_arr[0]=(uintptr_t)speaker; ship_list[1]=(uintptr_t)ship_arr; ship_list[2]=1; point(local_star,0x2c,ship_list); point(remote_star,0x2c,ship_list);
 FHDNPC_DialogueModeSet(2);
 FHDNPC_LocalDialogue(FHD_DLG_AGREEMENT,speaker,19,0,100,1);
 CHECK(!deliveries&&!FHDNPC_DialogueQueued()&&stats[4]==1);
 point(speaker,0x24,local_star);
 FHDNPC_LocalDialogue(FHD_DLG_LOOT_DESTROYED,speaker,19,0,100,2);
 CHECK(!deliveries&&!FHDNPC_DialogueQueued());
 FHDNPC_LocalDialogue(FHD_DLG_LOOT_DESTROYED,speaker,19,1,100,2);
 CHECK(deliveries==1&&information==1&&targets_linked==1&&!lock_depth);
 const uint16_t prefix[]=u"<color=187,154,38>корсар Ларс:</color> ";
 for(unsigned i=0;prefix[i];i++)CHECK(displayed[0][i]==prefix[i]);
 FHDNPC_LocalDialogue(FHD_DLG_LOOT_DESTROYED,speaker,19,1,100,3);
 CHECK(deliveries==2&&information==2&&different(displayed[0],displayed[1]));
 FHDNPC_LocalDialogue(FHD_DLG_LOOT_DESTROYED,speaker,19,1,100,3);
 CHECK(deliveries==2);
 fail_allocation=1;
 FHDNPC_LocalDialogue(FHD_DLG_AGREEMENT,speaker,19,0,100,4);
 CHECK(FHDNPC_DialogueQueued()==1);
 fail_allocation=0;
 point(player,0x24,remote_star);
 CHECK(!FHDNPC_NewsPump()&&!FHDNPC_DialogueQueued());
 point(speaker,0x24,remote_star);
 player[0x459]=1;
 FHDNPC_LocalDialogue(FHD_DLG_AGREEMENT,speaker,19,0,100,5);
 CHECK(deliveries==2&&!FHDNPC_DialogueQueued());
 player[0x459]=0;
 FHDNPC_LocalDialogue(FHD_DLG_AGREEMENT,speaker,19,0,100,5);
 CHECK(deliveries==3&&information==3);
 return 0;
}
void _start(void){
 int result=run();
 __asm__ volatile ("int $0x80"::"a"(1),"b"(result):"memory");
 __builtin_unreachable();
}
