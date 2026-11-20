/* FHD NPC AI 0.9 EXPERIMENTAL save thread integration. Only for exact SHA-256
   e43a4b15... Only TSaver.Execute VMT is changed, original always called.
   Memory is snapshotted from game's OnSave script on the game thread;
   committed on the worker after a target SAV CONTENTS change is confirmed.
   This file deliberately never writes to a game's .sav bytes. */
#include <stdint.h>
#include <stddef.h>
#include "fhd_npc_bridge.h"
#include "fhd_save_tag.h"
#if !defined(_WIN32) || !defined(_M_IX86)
#error Win32 x86 only
#endif
#define WIN __declspec(dllimport)
#define ST __stdcall
#define API __declspec(dllexport)
WIN void * ST GetModuleHandleW(const uint16_t*);
WIN int ST VirtualProtect(void*,uint32_t,uint32_t,uint32_t*);
WIN void ST Sleep(uint32_t);
WIN void * ST CreateFileW(const uint16_t*,uint32_t,uint32_t,void*,uint32_t,uint32_t,void*);
WIN int ST ReadFile(void*,void*,uint32_t,uint32_t*,void*);
WIN int ST CloseHandle(void*);
WIN int ST MultiByteToWideChar(uint32_t,uint32_t,const char*,int,uint16_t*,int);
#define PAGE_EXECUTE_READWRITE 0x40u
#define GENERIC_READ 0x80000000u
#define SHARE_ALL 7u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define CAP 4194240u
#define PATHCAP 2048u
static volatile uint32_t g_lock;
static uint8_t g_stage[CAP],g_current[CAP];
static uint32_t g_stage_size,g_current_size;
static uint64_t g_stage_key,g_current_key,g_initial_tag;
static uint32_t g_ready,g_active,g_armed;
static uint16_t g_current_path[PATHCAP],g_load_path[PATHCAP];
static uint16_t g_quick_src[PATHCAP],g_quick_dst[PATHCAP];
static uint32_t g_staged,g_committed,g_failed,g_loaded,g_mismatch,g_overwritten;
static uintptr_t g_base;
void *fhd_save_original;
extern void FHDNPC_LogSaveEvent(int32_t);
extern void FHDNPC_SaveStub(void);
static void lock(void){while(__atomic_exchange_n(&g_lock,1u,__ATOMIC_ACQUIRE))Sleep(0);}
static void unlock(void){__atomic_store_n(&g_lock,0u,__ATOMIC_RELEASE);}
static void cp(void*d,const void*s,uint32_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}
static void clear_path(uint16_t*p){p[0]=0;}
static int clone_path(uint16_t*out,const uint16_t*src){
 uint32_t n=0;if(!src)return 0;
 while(n<PATHCAP-1u && src[n]){if(src[n]<32u)return 0;out[n]=src[n];n++;}
 if(n<4u||n>=PATHCAP-1u)return 0;out[n]=0;return 1;
}
static uint64_t galaxy_key(void){
 uint8_t *galaxy;
 if(!g_base)return 0;
 galaxy=*(uint8_t**)(g_base+0x41d2a0u);
 if(!galaxy)return 0;
 {uint32_t seed=*(uint32_t*)(galaxy+0x58u);
  if(!seed)return 0;
  return ((uint64_t)0x4648444eu<<32)|seed;}
}
/* Full byte tag, two independent 32-bit hashes and a 512MiB hard read limit.
   A mismatch fails closed rather than attaching memory to an unrelated SAV. */
static uint64_t sav_tag(const uint16_t *path){
 uint32_t n=0,i,bytes=0,h1=2166136261u,h2=5381u;int ok=0;
 uint8_t buffer[2048];
 void*f=CreateFileW(path,GENERIC_READ,SHARE_ALL,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(f==(void*)(intptr_t)-1)return 0;
 for(;;){
   if(!ReadFile(f,buffer,sizeof(buffer),&n,0))break;
   if(!n){ok=1;break;}
   if(bytes>536870912u-n)break;
   bytes+=n;
   for(i=0;i<n;i++){h1=(h1^buffer[i])*16777619u;h2=((h2<<5)+h2)^buffer[i];}
 }
 CloseHandle(f);
 if(!ok||!bytes)return 0;
 return ((uint64_t)h1<<32)|h2;
}
/* Called from BV.OnSave after game has made its own snapshot but before it
   dispatches the async worker. Last staged state replaces an unqueued capture. */
API int32_t FHDNPC_StageSaveSnapshot(void){
 int32_t size;uint64_t key;
 if(!g_armed)return 0;
 key=galaxy_key();if(!key)return -1;
 lock();
 size=FHD_Snapshot(0,0);
 if(size<1||(uint32_t)size>CAP){g_failed++;unlock();return -2;}
 /* Ambiguous back-to-back OnSave before the async writer starts: never
    replace the first SAV's snapshot with a later one. Drop both instead.
    An absent sidecar is safer than attaching another turn's memories. */
 if(g_ready){g_ready=2;g_overwritten++;g_failed++;unlock();return 0;}
 if(FHD_Snapshot(g_stage,CAP)!=size){g_failed++;g_ready=0;unlock();return -3;}
 g_stage_size=(uint32_t)size;g_stage_key=key;g_ready=1;g_staged++;
 unlock();return 1;
}
/* Writer thread begins; capturing before TSaver.Execute keeps path and state
   tied to the same serial worker invocation. */
void FHDNPC_SaverBefore(void*save_worker){
 const uint16_t *path=0;uint32_t i;
 if(!g_armed||!save_worker)return;
 path=*(const uint16_t**)((uint8_t*)save_worker+0x2cu);
 lock();g_active=0;
 if(g_ready==2){g_ready=0;g_failed++;unlock();return;}
 if(g_ready==1 && clone_path(g_current_path,path)){
   g_current_size=g_stage_size;g_current_key=g_stage_key;
   cp(g_current,g_stage,g_stage_size);g_ready=0;g_active=1;
 }else if(g_ready){g_failed++;g_ready=0;}
 unlock();
 if(g_active)g_initial_tag=sav_tag(g_current_path);
 (void)i;
}
/* Writer already rotated the SAVs. Mirror sidecars only if each newly
   moved SAV still matches the content fingerprint recorded in the sidecar. */
static void rotate_quick(const uint16_t *path){
 static const char suffix[]="QuickSave.sav";
 uint32_t len=0,n,i,j,stem;
 uint16_t *src=g_quick_src,*dst=g_quick_dst;
 if(!g_base)return;
 while(len<PATHCAP-1u&&path[len])len++;
 if(len<sizeof(suffix)-1u||len>=PATHCAP-22u)return;
 stem=len-(sizeof(suffix)-1u);
 for(i=0;i<sizeof(suffix)-1u;i++){
   uint16_t c=path[stem+i];
   if(c>='a'&&c<='z')c=(uint16_t)(c-32);
   {char d=suffix[i];if(d>='a'&&d<='z')d=(char)(d-32);
    if(c!=(uint16_t)d)return;}
 }
 /* The native save writer rotates only these slots, 1..N+1. */
 n=*(uint32_t*)(g_base+0x40a8e4u);
 if(!n||n>20u)return;
 for(i=n;i>=1u;i--){
    uint32_t z=stem,k=i,cc=0;uint16_t rev[12];
    for(j=0;j<stem;j++){src[j]=path[j];dst[j]=path[j];}
    z=stem;
    for(j=0;j<9u;j++)src[z++]=(uint16_t)suffix[j]; /* QuickSave */
    if(i>1u){
      do{rev[cc++]=(uint16_t)('0'+k%10u);k/=10u;}while(k&&cc<12);
      while(cc)src[z++]=rev[--cc];
    }
    for(j=9u;j<sizeof(suffix)-1u;j++)src[z++]=(uint16_t)suffix[j];src[z]=0;
    z=stem;for(j=0;j<9u;j++)dst[z++]=(uint16_t)suffix[j];
    k=i+1u;cc=0;
    do{rev[cc++]=(uint16_t)('0'+k%10u);k/=10u;}while(k&&cc<12);
    while(cc)dst[z++]=rev[--cc];
    for(j=9u;j<sizeof(suffix)-1u;j++)dst[z++]=(uint16_t)suffix[j];dst[z]=0;
    {uint64_t current=sav_tag(dst);
      if(current)FHD_BridgeMoveTaggedSlotIfMatch(src,dst,current);}
    if(i==1u)break;
 }
}
void FHDNPC_SaverAfter(void*save_worker){
 uint64_t tag;int32_t r;
 (void)save_worker;
 if(!g_active)return;
 tag=sav_tag(g_current_path);
 if(!tag||(g_initial_tag&&tag==g_initial_tag)){
   g_failed++;g_active=0;FHDNPC_LogSaveEvent(2);return;
 }
 rotate_quick(g_current_path);
 r=FHD_BridgeSaveTaggedSlot(g_current_path,g_current_key,g_current,g_current_size,tag);
 if(r==1){g_committed++;FHDNPC_LogSaveEvent(1);}else{g_failed++;FHDNPC_LogSaveEvent(3);}
 g_active=0;
}
/* Loaded save filename is a Delphi AnsiString at a verified GlobalsV address.
   Do not trust a stale filename; file content MUST match saved tag. */
API int32_t FHDNPC_RestoreLoadedSnapshot(void){
 const char *ansi;uint64_t key,tag;int n,r;
 FHD_Reset();
 if(!g_armed||!g_base)return 0;
 ansi=*(const char**)(g_base+0x419df4u);
 if(!ansi||!ansi[0]){g_failed++;return -1;}
 n=MultiByteToWideChar(0,0,ansi,-1,g_load_path,PATHCAP);
 if(n<2||(uint32_t)n>=PATHCAP){g_failed++;return -2;}
 key=galaxy_key();tag=sav_tag(g_load_path);
 if(!key||!tag){g_failed++;return -3;}
 r=FHD_BridgeLoadTaggedSlot(g_load_path,key,tag);
 if(r==1)g_loaded++;else{g_mismatch++;g_failed++;}
 return r;
}
static int patch(void**slot,void*before,void*after){
 uint32_t old=0,unused=0;
 if(!VirtualProtect(slot,4u,PAGE_EXECUTE_READWRITE,&old))return 0;
 if(*slot!=before){VirtualProtect(slot,4u,old,&unused);return 0;}
 *(volatile void**)slot=after;
 VirtualProtect(slot,4u,old,&unused);return 1;
}
API int32_t FHDNPC_ArmSaveObserver(void){
 uint8_t*base=(uint8_t*)GetModuleHandleW(0);void**slot;
 if(g_armed)return 1;
 if(!base)return 0;
 slot=(void**)(base+0x1e51a4u);
 if(*slot!=(void*)(base+0x1e51c4u))return 0;
 if(base[0x1e51c4u]!=0x55u||base[0x1e51c5u]!=0x8bu||base[0x1e51c6u]!=0xecu)return 0;
 fhd_save_original=base+0x1e51c4u;
 if(!patch(slot,fhd_save_original,FHDNPC_SaveStub))return 0;
 g_base=(uintptr_t)base;g_armed=1;return 1;
}
API int32_t FHDNPC_DisarmSaveObserver(void){
 uint8_t*base=(uint8_t*)g_base;
 if(!g_armed||!base)return 0;
 if(*(void**)(base+0x1e51a4u)==(void*)FHDNPC_SaveStub)
   patch((void**)(base+0x1e51a4u),FHDNPC_SaveStub,fhd_save_original);
 g_armed=0;g_ready=0;g_active=0;return 1;
}
API void FHDNPC_ResetSaveStage(void){
 lock();g_ready=0;g_active=0;g_stage_size=0;g_current_size=0;
 g_staged=g_committed=g_failed=g_loaded=g_mismatch=g_overwritten=0;
 clear_path(g_current_path);unlock();
}
API void FHDNPC_SaveStats(uint32_t *v){
 if(!v)return;v[0]=g_staged;v[1]=g_committed;v[2]=g_failed;
 v[3]=g_loaded;v[4]=g_mismatch;v[5]=g_overwritten;
}
