/* VMT decision bridges for the exact supported Delphi executable. */
#include <stdint.h>
#include <stddef.h>
#include "fhd_live_policy.h"
#include "fhd_dialogue.h"
#include "fhd_native_duties.h"
#if !defined(_WIN32) || !defined(_M_IX86)
#error Win32 x86 only
#endif
#define API __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#define STDCALL __stdcall
DLLIMPORT int STDCALL VirtualProtect(void*,uint32_t,uint32_t,uint32_t*);
DLLIMPORT void* STDCALL GetModuleHandleW(const uint16_t*);
DLLIMPORT void* STDCALL CreateFileW(const uint16_t*,uint32_t,uint32_t,void*,uint32_t,uint32_t,void*);
DLLIMPORT int STDCALL WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
DLLIMPORT int STDCALL CloseHandle(void*);
DLLIMPORT uint32_t STDCALL GetTempPathW(uint32_t,uint16_t*);

#define PAGE_EXECUTE_READWRITE 0x40u
#define FILE_APPEND_DATA 4u
#define FILE_SHARE_READ 1u
#define OPEN_ALWAYS 4u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define COUNT 6u
#define TRANCLUCATOR_CATEGORY 4u
#define MAX_SAMPLES 8192u
/* Determined from the supplied EXE (SHA-256 e43a..ebf) and the VMT slot 0x1C. */
static const uint32_t k_vmt_slot_rva[COUNT]={0x310c8cu,0x1ab5a8u,0x1992ccu,0x1a2b2cu,0x1b8140u,0x29a3c4u};
static const uint32_t k_original_rva[COUNT]={0x31264cu,0x1ac298u,0x199fb4u,0x1a3994u,0x1b8c14u,0x29c1f8u};
extern void FHDNPC_Stub0(void); extern void FHDNPC_Stub1(void);
extern void FHDNPC_Stub2(void); extern void FHDNPC_Stub3(void);
extern void FHDNPC_Stub4(void); extern void FHDNPC_Stub5(void);
void* fhd_orig0=0;void* fhd_orig1=0;void* fhd_orig2=0;
void* fhd_orig3=0;void* fhd_orig4=0;void* fhd_orig5=0;
static void **g_originals[COUNT]={&fhd_orig0,&fhd_orig1,&fhd_orig2,&fhd_orig3,&fhd_orig4,&fhd_orig5};
static void* g_stubs[COUNT]={FHDNPC_Stub0,FHDNPC_Stub1,FHDNPC_Stub2,FHDNPC_Stub3,FHDNPC_Stub4,FHDNPC_Stub5};
static uint32_t g_counts[COUNT];
static uint32_t g_valid[COUNT],g_protected[COUNT],g_total_unique,g_reused;
static uint64_t g_seen[MAX_SAMPLES];
static uintptr_t g_base=0;
static int g_armed=0;
static int g_busy=0;
static uint16_t g_log_path[1100];

static void write_ascii_log(const char*line){
  static const uint16_t filename[]={'F','H','D','_','N','P','C','_','A','I','_','v','m','t','.','l','o','g',0};
  uint32_t n=GetTempPathW(1024,g_log_path),out=0,i=0,len=0;
  char text[240]; void* file;
  if(!n||n+sizeof(filename)/2>=1100)return;
  for(i=0;i<sizeof(filename)/2;i++)g_log_path[n+i]=filename[i];
  file=CreateFileW(g_log_path,FILE_APPEND_DATA,FILE_SHARE_READ,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(file==(void*)(intptr_t)-1)return;
  while(line[len]&&len<sizeof(text)-3){text[len]=line[len];len++;}
  text[len++]='\r';text[len++]='\n';
  WriteFile(file,text,len,&out,0);
  CloseHandle(file);
}
static void append_unsigned(char*str,uint32_t*pos,uint32_t cap,uint32_t value){
  char tmp[12];uint32_t n=0;
  do{tmp[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<sizeof(tmp));
  while(n&&(*pos+1)<cap)str[(*pos)++]=tmp[--n];
}
static void write_counters(void){
  const char *prefix="NPC VMT calls ranger,transport,pirate,warrior,trancl,stations=";
  uint32_t i,j=0;char line[240];
  while(prefix[j]&&j<sizeof(line)-1){line[j]=prefix[j];j++;}
  for(i=0;i<COUNT;i++){
    append_unsigned(line,&j,(uint32_t)sizeof(line),g_counts[i]);
    if(i+1<COUNT && j+1<sizeof(line))line[j++]=',';
  }
  if(j+20<sizeof(line)){
    const char*s="; unique=";i=0;while(s[i])line[j++]=s[i++];
    append_unsigned(line,&j,(uint32_t)sizeof(line),g_total_unique);
  }
  line[j]=0;write_ascii_log(line);
}
/* Callbacks execute on game's decision thread; an in-process aggregation only. */
void FHDNPC_OnShipTick(void *ship,uint32_t which){
  uint8_t *p=(uint8_t*)ship;
  uint32_t id,seed,type,owner,tag;
  uint64_t key;uint32_t idx,j;
  if(!g_armed||!ship||which>=COUNT||which==TRANCLUCATOR_CATEGORY)return;
  g_counts[which]++;
  /* Actual VMT must be the exact one patched. This prevents alternate subclasses
     with inherited methods from being assumed to be classified vanilla objects. */
  if(*(uintptr_t*)p!=g_base+k_vmt_slot_rva[which]-0x1cu){g_protected[which]++;return;}
  type=p[0x10u];owner=p[0x11u];
  if(type==0u||type==12u||owner==5u||owner==6u||owner>7u){g_protected[which]++;return;}
  if(which<5u && type!=which+1u){ /* ranger 1, transport 2, pirate 3, warrior 4, trancl 5 */
    g_protected[which]++;return;
  }
  if(which==5u && (type<6u||type>11u)){ /* excludes dominion/custom station */
    g_protected[which]++;return;
  }
  if(*(uintptr_t*)(p+0x3ccu) || p[0x44cu] || p[0x43cu]){
    g_protected[which]++;return;
  }
  id=*(uint32_t*)(p+0x04u);seed=*(uint32_t*)(p+0xe0u);
  if(!id||id==0xffffffffu)return;
  g_valid[which]++;
  key=((uint64_t)id<<32)|seed;
  idx=(id*2654435761u ^ seed) & (MAX_SAMPLES-1u);
  for(j=0;j<MAX_SAMPLES;j++){
    uint32_t n=(idx+j)&(MAX_SAMPLES-1u);
    if(!g_seen[n]){g_seen[n]=key;g_total_unique++;break;}
    if(g_seen[n]==key)break;
  }
  (void)tag;
}
/* Called from assembly with per-invocation storage; never sends a game order.
   Only proven original VMT dispatches reach the live policy. */
void FHDNPC_BeforeShipTick(void *ship,uint32_t category,FHDLiveFrame *frame){
  uint8_t *p=(uint8_t*)ship;
  if(frame){uint8_t*b=(uint8_t*)frame;uint32_t i;
    for(i=0;i<sizeof(*frame);i++)b[i]=0;}
  FHDNPC_OnShipTick(ship,category);
  if(!g_armed||!p||!frame||category>=COUNT||category==TRANCLUCATOR_CATEGORY)return;
  if(*(uintptr_t*)p!=g_base+k_vmt_slot_rva[category]-0x1cu)return;
  FHDNPC_LiveBefore(p,category,frame);
}
void FHDNPC_AfterShipTick(void *ship,uint32_t category,FHDLiveFrame *frame){
  if(frame&&frame->valid){
    FHDNPC_LiveAfter((uint8_t*)ship,category,frame);
    FHDNPC_DutyAfter(ship,category);
    if(category==5u){extern void FHDNPC_StationTick(void*);FHDNPC_StationTick(ship);}
    if(category==1u){extern void FHDNPC_TransportSalvage(void*);FHDNPC_TransportSalvage(ship);}
  }
  if(ship&&category<4u){extern void FHDNPC_GrievanceTick(void*,uint32_t); FHDNPC_GrievanceTick(ship,category);}
  if(FHDNPC_DialogueQueued())(void)FHDNPC_NewsPump();
}
static void write_live_counters(void){
  FHDLiveStats x;char line[240];uint32_t pos=0,i;
  const char *prefix="NPC AI live considered,registered,excluded,ranger_choices,ranger_applied,kills=";
  FHDNPC_LiveStatsGet(&x);
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  {uint32_t v[6]={x.considered,x.registered,x.excluded,x.ranger_choices,
        x.ranger_applied,x.confirmed_kills};
   for(i=0;i<6;i++){
      append_unsigned(line,&pos,(uint32_t)sizeof(line),v[i]);
      if(i<5&&pos+1<sizeof(line))line[pos++]=',';
   }}
  line[pos]=0;write_ascii_log(line);
  pos=0;prefix="NPC AI actions none,trade,piracy,fight,repair,flee,defend,explore,diplomacy,mission=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  for(i=0;i<10;i++){
     append_unsigned(line,&pos,(uint32_t)sizeof(line),x.action_counts[i]);
     if(i<9&&pos+1<sizeof(line))line[pos++]=',';
  }
  line[pos]=0;write_ascii_log(line);
  pos=0;prefix="NPC AI 18 skips invalid,provenance,scripts,special,identity,profile,no_action,unsupported,busy,pref,no_effect,pressure=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  { uint32_t v[12]={x.skip_invalid,x.skip_provenance,x.skip_scripts,x.skip_special,
     x.skip_identity,x.skip_profile,x.skip_no_action,x.skip_unsupported,x.skip_busy,
     x.skip_invalid_preferences,x.skip_no_effect,x.skip_invalid_pressure};
    for(i=0;i<12;i++){
      append_unsigned(line,&pos,(uint32_t)sizeof(line),v[i]);
      if(i<11&&pos+1<sizeof(line))line[pos++]=',';
    }
  }
  line[pos]=0;write_ascii_log(line);
  pos=0;prefix="NPC AI 18 categories ranger,transport,pirate,warrior,trancl,station=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  for(i=0;i<6;i++){
    append_unsigned(line,&pos,(uint32_t)sizeof(line),x.categories[i]);
    if(i<5&&pos+1<sizeof(line))line[pos++]=',';
  }
  prefix="; pirates_applied=";for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  append_unsigned(line,&pos,(uint32_t)sizeof(line),x.changed_pirates);
  prefix="; native_writes_preserved=";for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  append_unsigned(line,&pos,(uint32_t)sizeof(line),x.game_writes_preserved);
  line[pos]=0;write_ascii_log(line);
  pos=0;prefix="NPC AI 18 relation targets,nonzero,decision_changed,applied=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  {uint32_t v[4]={x.relationship_target_eligible,x.relationship_target_nonzero,
    x.relationship_action_changed,x.relationship_action_applied};
   for(i=0;i<4;i++){append_unsigned(line,&pos,sizeof(line),v[i]);
     if(i<3&&pos+1<sizeof(line))line[pos++]=',';}}
  line[pos]=0;write_ascii_log(line);
  pos=0;prefix="NPC AI 18 changes ranger,pirate,total; candidates transport,warrior; changed transport,warrior=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  {uint32_t v[7]={x.ranger_applied,x.changed_pirates,x.ranger_applied+x.changed_pirates,
           x.category_choices[1],x.category_choices[3],x.category_applied[1],x.category_applied[3]};
   for(i=0;i<7;i++){append_unsigned(line,&pos,sizeof(line),v[i]);
     if(i<6&&pos+1<sizeof(line))line[pos++]=',';}
  }
  line[pos]=0;write_ascii_log(line);
}
/* Compare the entire VMT table before any change. If any slot is unexpected,
   install NOTHING. Only this exact executable can pass the SHA gate in Init(). */
static int can_arm(uint8_t*base){
  uint32_t i;for(i=0;i<COUNT;i++){
    if(i==TRANCLUCATOR_CATEGORY)continue;
    void**slot=(void**)(base+k_vmt_slot_rva[i]);
    if(*slot!=(void*)(base+k_original_rva[i]))return 0;
    if((*(uint8_t*)(base+k_original_rva[i])!=0x55)||
       (*(uint8_t*)(base+k_original_rva[i]+1u)!=0x8b)||
       (*(uint8_t*)(base+k_original_rva[i]+2u)!=0xec))return 0;
  }
  return 1;
}
static int patch(void**slot,void*expected,void*value){
  uint32_t old=0,unused=0;
  if(!VirtualProtect(slot,4u,PAGE_EXECUTE_READWRITE,&old))return 0;
  if(*slot!=expected){VirtualProtect(slot,4u,old,&unused);return 0;}
  /* Volatile aligned DWORD store is atomic on x86. */
  *(volatile void**)slot=value;
  VirtualProtect(slot,4u,old,&unused);
  return 1;
}
API int32_t FHDNPC_DisarmObserver(void){
  uint32_t i;uint8_t*base=(uint8_t*)g_base;
  if(!g_armed||!base)return 0;
  for(i=0;i<COUNT;i++){
    if(i==TRANCLUCATOR_CATEGORY)continue;
    void**slot=(void**)(base+k_vmt_slot_rva[i]);
    /* If a different mod replaced our hook, do not overwrite its pointer. */
    if(*slot==g_stubs[i])patch(slot,g_stubs[i],*g_originals[i]);
  }
  g_armed=0;
  return 1;
}
extern int32_t FHDNPC_DisarmSaveObserver(void);
extern int32_t FHDNPC_DisarmSocial(void);
extern void FHDNPC_DisarmTrade(void);
extern void FHDNPC_ResetTradeStats(void);
extern void FHDNPC_TradeStats(uint32_t *out);
void FHDNPC_OnDllDetach(void){FHD_SettingsDisarm();FHDNPC_DisarmDuties();FHDNPC_DisarmTrade();extern int32_t FHDNPC_DisarmGrievance(void);(void)FHDNPC_DisarmGrievance();(void)FHDNPC_DisarmSocial();(void)FHDNPC_DisarmSaveObserver();(void)FHDNPC_DisarmObserver();}
API void FHDNPC_ResetCounters(void){
  FHDNPC_ResetTradeStats();
  FHDNPC_ResetDutyStats();
  uint32_t i;for(i=0;i<COUNT;i++){g_counts[i]=0;g_valid[i]=0;g_protected[i]=0;}
  for(i=0;i<MAX_SAMPLES;i++)g_seen[i]=0;
  g_total_unique=0;g_reused=0;FHDNPC_LiveReset();
  {extern void FHDNPC_ResetSocial(void);FHDNPC_ResetSocial();}
  {extern void FHDNPC_ResetGrievance(void);FHDNPC_ResetGrievance();}
}
API int32_t FHDNPC_ArmObserver(void){
  uint8_t*base=(uint8_t*)GetModuleHandleW(0);uint32_t i,j;
  if(g_armed)return 1;
  if(!base||g_busy)return 0;
  g_busy=1;g_base=(uintptr_t)base;
  if(!can_arm(base)){g_busy=0;write_ascii_log("FAIL: VMT fingerprints mismatch, no hooks");return 0;}
  for(i=0;i<COUNT;i++)*g_originals[i]=(void*)(base+k_original_rva[i]);
  for(i=0;i<COUNT;i++){
    if(i==TRANCLUCATOR_CATEGORY)continue;
    if(!patch((void**)(base+k_vmt_slot_rva[i]),*g_originals[i],g_stubs[i])){
      for(j=0;j<i;j++)if(j!=TRANCLUCATOR_CATEGORY)
        patch((void**)(base+k_vmt_slot_rva[j]),g_stubs[j],*g_originals[j]);
      g_busy=0;write_ascii_log("FAIL: VMT hook collision, all owned hooks removed");return 0;
    }
  }
  g_armed=1;g_busy=0;
  write_ascii_log("OK: 5 NPC VMT bridges armed");
  return 1;
}
API int32_t FHDNPC_ObserverStatus(void){return g_armed?1:0;}
extern void FHDNPC_SocialStats(uint32_t *);
void FHDNPC_LogMemoryAudit(const char *phase);
extern int32_t FHDNPC_StageSaveSnapshot(void);
extern void FHDNPC_SaveStats(uint32_t*);
API int32_t FHDNPC_SaveProbe(void){
  int32_t size,staged;
  uint32_t savestat[6];
  uint32_t pos=0,i;
  char line[160];const char *prefix="NPC AI memory snapshot_bytes=";
  if(!g_armed)return 0;
  staged=FHDNPC_StageSaveSnapshot();
  write_counters();write_live_counters();
  {
    uint32_t news[6],q=0;
    char msg[155];const char *label="NPC AI 213 radio mode,handed_to_ui,failed,bad_signature,outside_system,queued=";
    FHDNPC_NewsStats(news);
    while(label[q]){msg[q]=label[q];q++;}
    const uint32_t nums[]={FHDNPC_DialogueModeGet(),news[1],news[2],news[3],news[4],news[5]};
    for(unsigned i=0;i<6;i++){append_unsigned(msg,&q,sizeof(msg),nums[i]);if(i<5)msg[q++]=',';}
    msg[q]=0;write_ascii_log(msg);
    {uint32_t inf[2];uint32_t n=0;char report[130];
     const char *tag="NPC AI 214 information delivered,failed=";
     extern void FHDNPC_InformationStats(uint32_t *);
     FHDNPC_InformationStats(inf);
     for(unsigned j=0;tag[j]&&n<sizeof(report)-16u;j++)report[n++]=tag[j];
     append_unsigned(report,&n,sizeof(report),inf[0]);report[n++]=',';
     append_unsigned(report,&n,sizeof(report),inf[1]);
     report[n]=0;write_ascii_log(report);
    }
  }
  {
    const char *name="NPC AI 20 scout,patrol,raid,liner,envoy,escort,distress,paid,cancelled,arrived,defended,requested=";
    char duty_line[240];
    uint32_t q=0,d[12];
    FHDNPC_DutyStats(d);
    while(name[q]){duty_line[q]=name[q];q++;}
    for(uint32_t j=0;j<12;j++){
      append_unsigned(duty_line,&q,sizeof(duty_line),d[j]);
      if(j<11&&q<sizeof(duty_line)-1)duty_line[q++]=',';
    }
    duty_line[q]=0;write_ascii_log(duty_line);
    name="NPC AI 20 world risk_systems,known_markets,shortage_markets,risk_reduced_by_supply_today=";
    FHDNPC_WorldStats(d);
    q=0;
    while(name[q]){duty_line[q]=name[q];q++;}
    for(uint32_t j=0;j<4;j++){
      append_unsigned(duty_line,&q,sizeof(duty_line),d[j]);
      if(j<3)duty_line[q++]=',';
    }
    duty_line[q]=0;write_ascii_log(duty_line);
  }
  size=FHD_Snapshot(0,0); /* diagnostic only; does not persist the SAV memory */
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  append_unsigned(line,&pos,(uint32_t)sizeof(line),size>0?(uint32_t)size:0u);
  prefix="; managed_agents=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  append_unsigned(line,&pos,(uint32_t)sizeof(line),FHD_Count());
  prefix=staged==1?"; automatic_sav_sidecar=STAGED":"; automatic_sav_sidecar=NOT_STAGED";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  line[pos]=0;write_ascii_log(line);
  {uint32_t d[42];
   uint32_t q=0,k;const char*name="NPC AI 18 social reacts,remembered,eligible,transport_avoided,warrior_avoided,dedup,protected,not_registered=";
   FHDNPC_SocialStats(d);
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=0;k<8;k++){append_unsigned(line,&q,sizeof(line),d[k]);
      if(k<7&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 selection calls,existing,no_new,flagship,orders,selfdef,target,profile=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=8;k<16;k++){append_unsigned(line,&q,sizeof(line),d[k]);
      if(k<15&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 policy friend_avoid,risk_avoid,allowed,unknown_strength,betrayals=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=16;k<21;k++){append_unsigned(line,&q,sizeof(line),d[k]);
      if(k<20&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 verified contracts,joint_combat,contract_skips=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=21;k<24;k++){append_unsigned(line,&q,sizeof(line),d[k]);
     if(k<23&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 transport selection calls,existing,no_new,protected_order,target,eligible=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=24;k<30;k++){append_unsigned(line,&q,sizeof(line),d[k]);
     if(k<29&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 warrior selection calls,existing,no_new,protected_order,target,eligible=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=30;k<36;k++){append_unsigned(line,&q,sizeof(line),d[k]);
     if(k<35&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
   q=0;name="NPC AI 18 escort confirmed,day_skips,ineligible; targets verified,linked,rejected=";
   for(k=0;name[k]&&q<sizeof(line)-2;k++)line[q++]=name[k];
   for(k=36;k<42;k++){append_unsigned(line,&q,sizeof(line),d[k]);
     if(k<41&&q+1<sizeof(line))line[q++]=',';}
   line[q]=0;write_ascii_log(line);
  }
  /* Extended v1.5 diagnostics from standalone APIs; keep older 42-dword
     social array and FHDLiveStats layout intact for all older consumers. */
  {uint32_t a[12],b[10],q,k;char msg[240];const char*prefix;
   extern void FHDNPC_Social140Stats(uint32_t*);
   extern void FHDNPC_Live140Stats(uint32_t*);
   FHDNPC_Social140Stats(a);FHDNPC_Live140Stats(b);
   prefix="NPC AI 18 override transport orders,existing,flagship,fear,dominator,allowed=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=0;k<6u;k++){append_unsigned(msg,&q,sizeof(msg),a[k]);if(k<5u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 override military orders,existing,flagship,fear,dominator,allowed=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=6u;k<12u;k++){append_unsigned(msg,&q,sizeof(msg),a[k]);if(k<11u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 relation direct friends,enemies,applied,busy_override,pirate_up,pirate_down,ranger_up,ranger_down,unchanged,fear=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=0;k<10u;k++){append_unsigned(msg,&q,sizeof(msg),b[k]);if(k<9u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
  }
  /* Additive v1.5 counters; fixed-size ABI and SAV byte layout remain intact. */
  {uint32_t a[15],pcounts[10],i,q;char msg[240];const char*prefix;
   extern void FHDNPC_Social150Stats(uint32_t*);
   extern void FHDNPC_ProfessionStats(uint32_t*);
   FHDNPC_Social150Stats(a);FHDNPC_ProfessionStats(pcounts);
   prefix="NPC AI 18 player attacks,relation_targets,partnerships=";
   q=0;for(i=0;prefix[i]&&q<sizeof(msg)-2;i++)msg[q++]=prefix[i];
   for(i=0;i<3u;i++){append_unsigned(msg,&q,sizeof(msg),a[i]);if(i<2u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 pirate motive calls,economic,revenge,clan,friends,veto,unknown=";
   q=0;for(i=0;prefix[i]&&q<sizeof(msg)-2;i++)msg[q++]=prefix[i];
   for(i=3u;i<10u;i++){append_unsigned(msg,&q,sizeof(msg),a[i]);if(i<9u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 pirate cunning avoided_strong_target=";
   q=0;for(i=0;prefix[i]&&q<sizeof(msg)-2;i++)msg[q++]=prefix[i];
   append_unsigned(msg,&q,sizeof(msg),a[14]);msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 professions unknown,ranger,trader,liner,diplomat,free_pirate,clan_pirate,warrior,flagship,station=";
   q=0;for(i=0;prefix[i]&&q<sizeof(msg)-2;i++)msg[q++]=prefix[i];
   for(i=0;i<10u;i++){append_unsigned(msg,&q,sizeof(msg),pcounts[i]);if(i<9u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
  }
  /* Additional 1.6 telemetry. ABI3 and the old stat calls are unchanged. */
  {uint32_t values[9],roles[5],q=0,k;char msg[240];const char*prefix;
   extern void FHDNPC_Social160Stats(uint32_t*);
   extern void FHDNPC_ExtraProfessionStats(uint32_t*);
   FHDNPC_Social160Stats(values);FHDNPC_ExtraProfessionStats(roles);
   prefix="NPC AI 18 survivor fear_checks,memory_fear,player,unknown; trust_checks,rejected; extortion_events,player; weighted_attacks=";
   for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=0;k<9u;k++){append_unsigned(msg,&q,sizeof(msg),values[k]);if(k<8u&&q+1<sizeof(msg))msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 extra professions agent,special_agent,smuggler,mercenary,spy=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=0;k<5u;k++){append_unsigned(msg,&q,sizeof(msg),roles[k]);if(k<4u&&q+1<sizeof(msg))msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
  }
  /* 1.7 exact-game observer diagnostics. Stat[14] counts HomePlanet
     penalty branches bypassed in verified, nonprovoked self-defense. */
  {uint32_t a[20],q,k;char msg[240];const char*prefix;
   extern void FHDNPC_GrievanceStats(uint32_t*);
   FHDNPC_GrievanceStats(a);
   prefix="NPC AI 18 firststrike npc,player,defense,denied,home_branch_skipped=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   {uint32_t nums[5]={a[0],a[1],a[2],a[3],a[14]};
    for(k=0;k<5u;k++){append_unsigned(msg,&q,sizeof(msg),nums[k]);if(k<4u)msg[q++]=',';}}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 18 loot taken,destroyed,protest_intent,compensation_intent,confront_intent=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=4u;k<9u;k++){append_unsigned(msg,&q,sizeof(msg),a[k]);if(k<8u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 213 loot player_taken,player_destroyed,npc_taken,npc_destroyed,missile_conflicts=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-2;k++)msg[q++]=prefix[k];
   for(k=15u;k<20u;k++){append_unsigned(msg,&q,sizeof(msg),a[k]);if(k<19u)msg[q++]=',';}
   msg[q]=0;write_ascii_log(msg);
   prefix="NPC AI 214 compensation queued,native_attempted=";
   q=0;for(k=0;prefix[k]&&q<sizeof(msg)-22;k++)msg[q++]=prefix[k];
   append_unsigned(msg,&q,sizeof(msg),a[7]);msg[q++]=',';
   append_unsigned(msg,&q,sizeof(msg),a[9]);
   msg[q]=0;write_ascii_log(msg);
  }
  /* Observable game conditions instead of fabricated in-system activity. */
  {uint32_t ra[4],pop[7],extra[1],claims[6],i=0,k=0;char line[240];
   extern void FHDNPC_StationStats(uint32_t*);
   extern void FHDNPC_PopulationStats(uint32_t*);
   extern void FHDNPC_GrievanceExtraStats(uint32_t*);
   extern void FHDNPC_CompensationDiagnostics(uint32_t*);
   FHDNPC_StationStats(ra);FHDNPC_PopulationStats(pop);FHDNPC_GrievanceExtraStats(extra);
   FHDNPC_CompensationDiagnostics(claims);
   const char *prefix="NPC AI 216 station incidents,reports,dispatches,overflow=";
   for(i=0;prefix[i]&&k<sizeof(line)-2;i++)line[k++]=prefix[i];
   for(i=0;i<4;i++){append_unsigned(line,&k,sizeof(line),ra[i]);if(i<3u)line[k++]=',';}
   line[k]=0;write_ascii_log(line);
   prefix="NPC AI 216 population systems,with_traffic,empty,space,docked,hyperspace,ships=";
   k=0;for(i=0;prefix[i]&&k<sizeof(line)-2;i++)line[k++]=prefix[i];
   for(i=0;i<7;i++){append_unsigned(line,&k,sizeof(line),pop[i]);if(i<6u)line[k++]=',';}
   line[k]=0;write_ascii_log(line);
   prefix="NPC AI 216 pirate retaliation orders=";
   k=0;for(i=0;prefix[i]&&k<sizeof(line)-2;i++)line[k++]=prefix[i];
   append_unsigned(line,&k,sizeof(line),extra[0]);line[k]=0;write_ascii_log(line);
   prefix="NPC AI 216 claims absent,busy,cooldown,contact,unavailable,attempts=";
   k=0;for(i=0;prefix[i]&&k<sizeof(line)-2;i++)line[k++]=prefix[i];
   for(i=0;i<6;i++){append_unsigned(line,&k,sizeof(line),claims[i]);if(i<5u)line[k++]=',';}
   line[k]=0;write_ascii_log(line);
  }
  {
    uint32_t trades[5],q=0;char msg[180];
    const char *prefix="NPC AI 18 trade bought,route_selected,sold,cancelled,vanilla=";
    FHDNPC_TradeStats(trades);
    for(i=0;prefix[i];i++)msg[q++]=prefix[i];
    for(i=0;i<5u;i++){append_unsigned(msg,&q,sizeof(msg),trades[i]);if(i<4u)msg[q++]=',';}
    msg[q]=0;write_ascii_log(msg);
  }
  {uint32_t s[2],q=0,i=0;char line[140];
   const char*prefix="NPC AI 217 salvage attempted,ordered=";
   extern void FHDNPC_TransportSalvageStats(uint32_t*);
   FHDNPC_TransportSalvageStats(s);
   for(i=0;prefix[i]&&q<sizeof(line)-2;i++)line[q++]=prefix[i];
   append_unsigned(line,&q,sizeof(line),s[0]);line[q++]=',';
   append_unsigned(line,&q,sizeof(line),s[1]);line[q]=0;
   write_ascii_log(line);
  }
  {uint32_t v[4],q=0,j;char buf[170];
   const char*prefix="NPC AI 218 station pairs,below_threshold,ready,reported=";
   extern void FHDNPC_StationDiagnostics(uint32_t*);
   FHDNPC_StationDiagnostics(v);
   for(j=0;prefix[j]&&q<sizeof(buf)-2;j++)buf[q++]=prefix[j];
   for(j=0;j<4u;j++){append_unsigned(buf,&q,sizeof(buf),v[j]);if(j<3u)buf[q++]=',';}
   buf[q]=0;write_ascii_log(buf);
  }
  {uint32_t v[9],q=0,j;char buf[200];
   const char*prefix="NPC AI 218 salvage eligible,no_space,no_hook,busy,unsafe,route,profile,throttled,attempts=";
   extern void FHDNPC_TransportSalvageDiagnostics(uint32_t*);
   FHDNPC_TransportSalvageDiagnostics(v);
   for(j=0;prefix[j]&&q<sizeof(buf)-2;j++)buf[q++]=prefix[j];
   for(j=0;j<9u;j++){append_unsigned(buf,&q,sizeof(buf),v[j]);if(j<8u)buf[q++]=',';}
   buf[q]=0;write_ascii_log(buf);
  }
  {uint32_t v[4],q=0,j;char buf[160];
   const char*prefix="NPC AI 218 extortion attempted,accepted,dialogue_counter,retained=";
   extern void FHDNPC_ClaimResults(uint32_t*);
   FHDNPC_ClaimResults(v);
   for(j=0;prefix[j]&&q<sizeof(buf)-2;j++)buf[q++]=prefix[j];
   for(j=0;j<4u;j++){append_unsigned(buf,&q,sizeof(buf),v[j]);if(j<3u)buf[q++]=',';}
   buf[q]=0;write_ascii_log(buf);
  }
  FHDNPC_LogMemoryAudit("SAVE");
  FHDNPC_SaveStats(savestat);
  pos=0;prefix="NPC AI 18 SAV staged,committed,failed,loaded,mismatch,overwritten=";
  for(i=0;prefix[i]&&pos<sizeof(line)-2;i++)line[pos++]=prefix[i];
  for(i=0;i<6;i++){append_unsigned(line,&pos,sizeof(line),savestat[i]);
      if(i<5&&pos+1<sizeof(line))line[pos++]=',';}
  line[pos]=0;write_ascii_log(line);
  return 1;
}

void FHDNPC_LogSaveEvent(int32_t kind){
  if(kind==1)write_ascii_log("NPC AI 18 SAV sidecar COMMITTED after successful writer");
  else if(kind==2)write_ascii_log("NPC AI 18 SAV writer FAILED/UNCHANGED; sidecar not committed");
  else if(kind==3)write_ascii_log("NPC AI 18 SAV sidecar WRITE FAILED");
}

/* Read-only audit logs exactly the relationship state used by FHD_Snapshot.
   Compare SAVE and RESTORED fingerprints for an unchanged SAV. */
void FHDNPC_LogMemoryAudit(const char *phase){
 uint32_t v[6],i,k=0,n=0;char line[200];
 const char *prefix="NPC AI 18 relationship ";
 if(FHD_RelationAudit(v,6u)!=1)return;
 for(i=0;prefix[i]&&n<sizeof(line)-2;i++)line[n++]=prefix[i];
 for(i=0;phase[i]&&n<sizeof(line)-2;i++)line[n++]=phase[i];
 prefix=" active,events,links,positive,negative,fingerprint=";
 for(i=0;prefix[i]&&n<sizeof(line)-2;i++)line[n++]=prefix[i];
 for(k=0;k<6u;k++){
   append_unsigned(line,&n,sizeof(line),v[k]);
   if(k<5u && n+1<sizeof(line))line[n++]=',';
 }
 line[n]=0;write_ascii_log(line);
}
