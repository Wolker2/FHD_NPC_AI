/* Use the game's radio channel, scoped to the player's actual system. */
#include <stdint.h>
#include "fhd_dialogue.h"
#if defined(FHD_NEWS_TEST) && defined(__i386__)
#define API
#define DLLIMPORT
#define STDCALL
#elif defined(_WIN32)
#define API __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#define STDCALL __stdcall
#else
#error Windows host only
#endif
DLLIMPORT void* STDCALL GetModuleHandleW(const uint16_t*);
DLLIMPORT uint16_t* STDCALL SysAllocStringLen(const uint16_t*,uint32_t);
DLLIMPORT void STDCALL SysFreeString(uint16_t*);
extern int32_t FHDNPC_Status(void);
extern void *FHDNPC_InvokeNews(void*,uint32_t,uint32_t,const uint16_t*);
#define NEWS_RVA 0x3a5dc0u
#define GET_PLAYER_RVA 0x347300u
#define GALAXY_RVA 0x41d2a0u
#define MESSAGE_LOCK 0x41cc74u
#define LOCK_ENTER 0x029678u
#define LOCK_LEAVE 0x029680u
#define MAX_MSG_LEN 650u
#define RADIO_KIND 1u
#define INFO_KIND 0u /* Globals.pas message_galaxy_news; journal/information */
#define INFO_MAX_LEN (MAX_MSG_LEN+32u)
static uint32_t stats[5]; /* attempted,accepted,failed,invalid_signature,outside_system */
static uint32_t info_stats[2]; /* information copies displayed, failures */
API void FHDNPC_InformationStats(uint32_t *out){
 if(out){out[0]=info_stats[0];out[1]=info_stats[1];}
}
static const uint8_t *image;
static uint32_t signature_checked;
static int signature_ok;
typedef void (__attribute__((regparm(3))) *FullName)(const uint8_t *,const uint16_t *,uint16_t **);
typedef void (__attribute__((regparm(3))) *Lock)(const uint8_t *);
static const struct {uint32_t byte_count;uint16_t text[2];} separator={2,{' ',0}};
static uint32_t number(const uint8_t *object,unsigned offset){
 return *(const uint32_t *)(object+offset);
}
static const uint8_t *pointer(const uint8_t *object,unsigned offset){
 return *(const uint8_t *const *)(object+offset);
}
static int matches(const uint8_t *data,const uint8_t *signature,unsigned size){
 for(unsigned i=0;i<size;i++)if(data[i]!=signature[i])return 0;
 return 1;
}
void FHDNPC_NewsReset(void){
 for(unsigned i=0;i<5u;i++)stats[i]=0;
 info_stats[0]=info_stats[1]=0;
 signature_checked=0;signature_ok=0;image=0;
}
API void FHDNPC_NewsStats(uint32_t*out){
 if(!out)return;
 for(unsigned i=0;i<5u;i++)out[i]=stats[i];
 out[5]=FHDNPC_DialogueQueued();
}
static int verify_entry(void){
 static const uint8_t news[]={0x55,0x8b,0xec,0x83,0xc4,0xf4,0x53,0x56,0x57,0x8b,0xf9},
                       player[]={0xa1,0xfc,0x00,0x81,0x00,0x35,0xd3,0x15,0xcd,0xb1,0xc3},
                       enter[]={0x8b,0x10,0xff,0x12,0xc3},
                       leave[]={0x8b,0x10,0xff,0x52,0x04,0xc3};
 if(signature_checked)return signature_ok;
 signature_checked=1;
 if(FHDNPC_Status()!=1)return 0;
 image=GetModuleHandleW(0);
 if(!image||!matches(image+NEWS_RVA,news,sizeof(news))||
    !matches(image+GET_PLAYER_RVA,player,sizeof(player))||
    !matches(image+LOCK_ENTER,enter,sizeof(enter))||!matches(image+LOCK_LEAVE,leave,sizeof(leave))){
   stats[3]++;return 0;
 }
 signature_ok=1;return 1;
}
static const uint8_t *player_system(void){
 if(!verify_entry())return 0;
 typedef const uint8_t *(*GetPlayer)(void);
 const uint8_t *player=((GetPlayer)(image+GET_PLAYER_RVA))();
 return player&&!player[0x459u]?pointer(player,0x24u):0; /* InHyperspace, CurrentStar */
}
void FHDNPC_LocalDialogue(uint32_t event,const uint8_t *speaker,uint32_t target,
                          uint32_t personal,uint32_t day,uint32_t salt){
 uint32_t mode=FHDNPC_DialogueModeGet();
 /* Ordinary ships can still contact the player personally, but they cannot
    issue public radio bulletins or create an information/news item. */
 if(!speaker||speaker[0x10u]>11u||
    mode==FHD_DLG_VANILLA||(!personal&&mode==FHD_DLG_PLAYER))return;
 if(speaker[0x10u]<6u && !personal)return;
 const uint8_t *star=player_system();
 uint32_t system=star?number(star,4u):0;
 FHD_DialogueContext(system,day);
 if(!system||star!=pointer(speaker,0x24u)){stats[4]++;return;}
 uint16_t *full_name=0;
 const uint8_t *vmt=pointer(speaker,0);
 FullName get_full_name=vmt?(FullName)pointer(vmt,0x28):0;
 if(get_full_name)get_full_name(speaker,separator.text,&full_name);
 int queued=FHD_DialogueNamed(event,speaker[0x408u],number(speaker,4u),target,personal,
    day,salt,system,full_name&&full_name[0]?full_name:(const uint16_t *)pointer(speaker,8u));
 SysFreeString(full_name);
 if(queued==1)(void)FHDNPC_NewsPump();
}
/* Keep personal ship-to-player radio separate from station-only news.
   Sender names and target identities come from the already verified dialogue
   queue. Inspect the current star's real ship list; don't assume a sender. */
static int station_sender(uint32_t sender,const uint8_t*star){
 if(!sender||!star)return 0;
 const uint8_t *list=pointer(star,0x2cu);
 if(!list)return 0;
 uint32_t n=number(list,8u);
 if(n>4096u)return 0;
 const uint8_t *const *ships=(const uint8_t *const *)pointer(list,4u);
 if(!ships)return 0;
 for(uint32_t i=0;i<n;i++){
   const uint8_t *p=ships[i];
   if(p && number(p,4u)==sender && pointer(p,0x24u)==star &&
      p[0x10u]>=6u && p[0x10u]<=11u)return 1;
 }
 return 0;
}
uint32_t FHDNPC_NewsPump(void){
 if(!FHDNPC_DialogueQueued())return 0;
 const uint8_t *star=player_system();
 const uint8_t *galaxy=star?pointer(image,GALAXY_RVA):0;
 uint32_t day=galaxy?number(galaxy,0x4cu):0;
 FHD_DialogueContext(star?number(star,4u):0,day);
 if(!day||day>1000000u||!FHDNPC_DialogueQueued())return 0;
 uint16_t line[MAX_MSG_LEN];
 uint32_t count=0;
 while(FHDNPC_DialogueQueued()){
   int32_t len=FHDNPC_DialoguePeekW(line,MAX_MSG_LEN);
   if(len<=0){stats[2]++;break;}
   uint16_t *text=SysAllocStringLen(line,(uint32_t)len);
   if(!text){stats[2]++;break;}
   const uint8_t *lock=pointer(image,MESSAGE_LOCK);
   if(!lock){SysFreeString(text);stats[2]++;break;}
   stats[0]++;
   /* The native routine also enters this recursive lock. Publish the text and
      the first ship target together before the UI can rebuild its buttons. */
   ((Lock)(image+LOCK_ENTER))(lock);
   void *entry=FHDNPC_InvokeNews((void *)(image+NEWS_RVA),RADIO_KIND,day,text);
   if(entry){
      /* TMessagePlayer.Targets[0].ShipId: click the message to locate sender. */
      *(uint32_t *)((uint8_t *)entry+0x24u)=FHD_DialogueSender();
      /* The stock panel upgrades radio with player when target[1] matches
         the player; world-only NPC messages are not falsely marked personal. */
      if(FHD_DialoguePersonal())*(uint32_t *)((uint8_t *)entry+0x2cu)=FHD_DialogueTarget();
      /* Information interface has its OWN native message kind and lifespan.
         An identical text is deduplicated by the engine regardless of kind,
         therefore this mirror has a different short prefix. */
      if(station_sender(FHD_DialogueSender(),star)){
      static const uint16_t info_prefix[]=u"Сводка станции: ";
      uint16_t info_line[INFO_MAX_LEN];
      uint32_t k=0,n=0;
      while(info_prefix[k] && k<INFO_MAX_LEN-1u){info_line[k]=info_prefix[k];k++;}
      while(n<(uint32_t)len && k+n<INFO_MAX_LEN-1u){info_line[k+n]=line[n];n++;}
      info_line[k+n]=0;
      uint16_t *info=SysAllocStringLen(info_line,k+n);
      if(info){
        void *notice=FHDNPC_InvokeNews((void *)(image+NEWS_RVA),INFO_KIND,day,info);
        if(notice){
          *(uint32_t *)((uint8_t *)notice+0x24u)=FHD_DialogueSender();
          info_stats[0]++;
        }else info_stats[1]++;
        SysFreeString(info);
      }else info_stats[1]++;
      }
   }
   ((Lock)(image+LOCK_LEAVE))(lock);
   SysFreeString(text);
   if(!entry){stats[2]++;break;}
   (void)FHDNPC_DialogueAcknowledge();
   stats[1]++;count++;
 }
 return count;
}

/* Official station bulletin, grounded in 3-4 distinct, deduplicated native
   ReactToAttack incidents. No ordinary ship speaks on the public channel. */
static uint32_t append16(uint16_t*out,uint32_t cap,uint32_t n,const uint16_t*src){
 if(!src)return n;
 for(uint32_t i=0;src[i]&&n+1u<cap&&i<180u;i++)out[n++]=src[i];
 out[n]=0;return n;
}
static uint32_t append_clean_name(uint16_t*out,uint32_t cap,uint32_t n,const uint16_t*src){
 uint32_t in_tag=0,start=n;
 if(!src)return n;
 for(uint32_t i=0;src[i]&&n+1u<cap&&i<120u;i++){
   uint16_t c=src[i];
   if(c=='<'){in_tag=1;continue;}
   if(c=='>'){in_tag=0;continue;}
   if(in_tag||c<32u)continue;
   if(c=='(' && src[i+1]==0x043du && src[i+2]==0x0430u && src[i+3]==0x0433u)break;
   if(c=='(' && (src[i+1]=='r'||src[i+1]=='R') && src[i+2]=='e' && src[i+3]=='w')break;
   out[n++]=c;
 }
 while(n>start && out[n-1]==' ')n--;
 out[n]=0;return n;
}
static uint32_t append_u32_16(uint16_t*out,uint32_t cap,uint32_t n,uint32_t val){
 uint16_t tmp[12];uint32_t len=0;
 do{tmp[len++]=(uint16_t)('0'+(val%10u));val/=10u;}while(val&&len<12u);
 while(len && n+1u<cap)out[n++]=tmp[--len];
 out[n]=0;return n;
}
static struct {uint32_t a,v,day;} attack_notice_seen[32];
static uint32_t attack_notice_idx;
uint32_t FHDNPC_AttackInfoNotice(const void *attacker,const void *victim,uint32_t day,
                                 uint32_t reason,uint32_t bounty){
 static const uint16_t c_mil[]=u"<color=120,190,255>Военный перехват:</color> ";
 static const uint16_t c_pir[]=u"<color=255,130,110>Пиратское нападение:</color> ";
 static const uint16_t c_bnt[]=u"<color=255,210,90>Охота по контракту:</color> ";
 static const uint16_t c_oth[]=u"<color=220,200,140>Боевой инцидент:</color> ";
 static const uint16_t verb[]=u" атакует ";
 static const uint16_t r_mil[]=u". Причина: розыск за нападения на суда и нарушение безопасности в системе.";
 static const uint16_t r_pir_rob[]=u". Причина: разбой и попытка грабежа в системе.";
 static const uint16_t r_pir_rev[]=u". Причина: личная месть за прошлый конфликт.";
 static const uint16_t r_bnt[]=u". Причина: разыскивается по контракту за нападение, награда ";
 static const uint16_t r_bnt_end[]=u" cr.";
 static const uint16_t r_oth[]=u". Причина: личный конфликт или ответ на враждебные действия.";
 static const uint16_t unk[]=u"неизвестный корабль";
 if(!attacker||!victim||FHDNPC_DialogueModeGet()==FHD_DLG_VANILLA||!verify_entry())return 0u;
 const uint8_t *star=player_system();
 const uint8_t *a=(const uint8_t*)attacker,*v=(const uint8_t*)victim;
 if(!star||star!=pointer(a,0x24u)||star!=pointer(v,0x24u))return 0u;
 uint32_t aid=number(a,4u),vid=number(v,4u);
 if(!aid||!vid||aid==vid)return 0u;
 for(uint32_t i=0;i<32u;i++){
   if(attack_notice_seen[i].a==aid&&attack_notice_seen[i].v==vid&&attack_notice_seen[i].day==day)return 0u;
 }
 attack_notice_seen[attack_notice_idx&31u].a=aid;
 attack_notice_seen[attack_notice_idx&31u].v=vid;
 attack_notice_seen[attack_notice_idx&31u].day=day;
 attack_notice_idx++;
 uint16_t *aname=0,*vname=0,line[MAX_MSG_LEN];
 FullName fn;
 const uint8_t *vmt=pointer(a,0u);
 fn=vmt?(FullName)pointer(vmt,0x28u):0;
 if(fn)fn(a,separator.text,&aname);
 vmt=pointer(v,0u);fn=vmt?(FullName)pointer(vmt,0x28u):0;
 if(fn)fn(v,separator.text,&vname);
 uint32_t k=0u;
 if(reason==1u)k=append16(line,MAX_MSG_LEN,k,c_mil);
 else if(reason==2u||reason==5u)k=append16(line,MAX_MSG_LEN,k,c_pir);
 else if(reason==3u)k=append16(line,MAX_MSG_LEN,k,c_bnt);
 else k=append16(line,MAX_MSG_LEN,k,c_oth);
 k=append_clean_name(line,MAX_MSG_LEN,k,aname&&aname[0]?aname:(const uint16_t*)pointer(a,8u));
 if(k==0u)k=append16(line,MAX_MSG_LEN,k,unk);
 k=append16(line,MAX_MSG_LEN,k,verb);
 k=append_clean_name(line,MAX_MSG_LEN,k,vname&&vname[0]?vname:(const uint16_t*)pointer(v,8u));
 if(reason==1u)k=append16(line,MAX_MSG_LEN,k,r_mil);
 else if(reason==2u)k=append16(line,MAX_MSG_LEN,k,r_pir_rob);
 else if(reason==5u)k=append16(line,MAX_MSG_LEN,k,r_pir_rev);
 else if(reason==3u){
   k=append16(line,MAX_MSG_LEN,k,r_bnt);
   k=append_u32_16(line,MAX_MSG_LEN,k,bounty);
   k=append16(line,MAX_MSG_LEN,k,r_bnt_end);
 }else k=append16(line,MAX_MSG_LEN,k,r_oth);
 SysFreeString(aname);SysFreeString(vname);
 uint16_t *msg=SysAllocStringLen(line,k);
 if(!msg)return 0u;
 const uint8_t *lock=pointer(image,MESSAGE_LOCK);
 if(!lock){SysFreeString(msg);return 0u;}
 ((Lock)(image+LOCK_ENTER))(lock);
 void *news=FHDNPC_InvokeNews((void*)(image+NEWS_RVA),INFO_KIND,day,msg);
 if(news){
   *(uint32_t*)((uint8_t*)news+0x24u)=aid;
   *(uint32_t*)((uint8_t*)news+0x2cu)=vid;
   info_stats[0]++;
 }else info_stats[1]++;
 ((Lock)(image+LOCK_LEAVE))(lock);
 SysFreeString(msg);
 return news?1u:0u;
}
uint32_t FHDNPC_StationBroadcast(const void *station,const void *offender,uint32_t day,uint32_t dispatched){
 static const uint16_t color[]=u"<color=187,154,38>";
 static const uint16_t after_name[]=u":</color> ";
 static const uint16_t *const ranger[10]={
   u"Зарегистрированы повторные нападения. Нарушитель: ",
   u"Зафиксирована серия нападений на корабли. Подозреваемый: ",
   u"Дежурная служба разыскивает участника нескольких атак: ",
   u"Поступило несколько подтверждённых сообщений о нападениях. Нападавший: ",
   u"Повторные атаки в системе требуют вмешательства. Разыскивается: ",
   u"Просим содействия в пресечении нападений. Подозреваемый: ",
   u"Объявляем предупреждение о неоднократных нападениях. Нарушитель: ",
   u"Нужна помощь рейнджеров в задержании нападавшего: ",
   u"Зафиксировано несколько нападений. Нападавший: ",
   u"В этой системе отмечена серия атак. Подозреваемый: "};
 static const uint16_t *const military[10]={
   u"Зафиксирована серия пиратских атак. Нарушитель: ",
   u"Внимание экипажам. Подтверждены неоднократные нападения, нападавший: ",
   u"Военное командование предупреждает о нападениях. Пират: ",
   u"При продолжении атак последуют ответные меры. Нарушитель: ",
   u"Зафиксированы повторные нарушения режима безопасности. Пират: ",
   u"Повторные нападения отмечены в системе. Подозреваемый: ",
   u"Военный патруль получил сведения о серии атак. Пират: ",
   u"Повторные атаки угрожают безопасности движения. Нападавший: ",
   u"Зафиксированы неоднократные атаки. Нарушитель: ",
   u"Предупреждаем участников движения о пиратских нападениях. Нарушитель: "};
 static const uint16_t sent[]=u" На перехват направлен военный корабль.";
 static const uint16_t title_ranger[]=u"Рейнджерский центр";
 static const uint16_t title_military[]=u"Военная база";
 static const uint16_t pirate_unknown[]=u"пиратский корабль";
 /* One station incident produces exactly one information notification.
    Allow station security bulletins in both FHD_DLG_PLAYER and FHD_DLG_LOCAL. */
 if(!station||!offender||FHDNPC_DialogueModeGet()==FHD_DLG_VANILLA||!verify_entry())return 0u;
 const uint8_t*star=player_system();
 if(!star||star!=pointer(station,0x24u)||star!=pointer(offender,0x24u))return 0u;
 if(number(offender,4u)==0u||number(station,4u)==0u)return 0u;
 uint16_t *station_name=0,*offender_name=0,line[MAX_MSG_LEN];
 FullName name;
 const uint8_t*vmt=pointer(station,0u);
 name=vmt?(FullName)pointer(vmt,0x28u):0;
 if(name)name(station,separator.text,&station_name);
 vmt=pointer(offender,0u);name=vmt?(FullName)pointer(vmt,0x28u):0;
 if(name)name(offender,separator.text,&offender_name);
 uint32_t k=0u,variant=(day^number(station,4u)^number(offender,4u))%10u;
 k=append16(line,MAX_MSG_LEN,k,color);
 k=append16(line,MAX_MSG_LEN,k,station_name&&station_name[0]?station_name:
     (((const uint8_t*)station)[0x10u]==6u?title_ranger:title_military));
 k=append16(line,MAX_MSG_LEN,k,after_name);
 k=append16(line,MAX_MSG_LEN,k,(((const uint8_t*)station)[0x10u]==6u?ranger:military)[variant]);
 k=append_clean_name(line,MAX_MSG_LEN,k,offender_name&&offender_name[0]?offender_name:pirate_unknown);
 if(dispatched && ((const uint8_t*)station)[0x10u]==8u)k=append16(line,MAX_MSG_LEN,k,sent);
 SysFreeString(station_name);SysFreeString(offender_name);
 uint16_t*msg=SysAllocStringLen(line,k);
 if(!msg)return 0u;
 const uint8_t*lock=pointer(image,MESSAGE_LOCK);
 if(!lock){SysFreeString(msg);return 0u;}
 ((Lock)(image+LOCK_ENTER))(lock);
 void*news=FHDNPC_InvokeNews((void*)(image+NEWS_RVA),INFO_KIND,day,msg);
 if(news){
   *(uint32_t*)((uint8_t*)news+0x24u)=number(station,4u);
   *(uint32_t*)((uint8_t*)news+0x2cu)=number(offender,4u);
   info_stats[0]++;
 }else info_stats[1]++;
 ((Lock)(image+LOCK_LEAVE))(lock);
 SysFreeString(msg);
 return news?1u:0u;
}
