/* Optional XenoNative handshake plus ScriptLibs exact-EXE SHA-256 gate.
 * Unrecognized executables never activate our game hooks.
 * No new threads, heap allocation or global UI interception. */
#include <stdint.h>
#include "fhd_npc_ai.h"
#include "fhd_dialogue.h"
#include "fhd_native_duties.h"
#include <stddef.h>

#if !defined(_WIN32)
#error This source is for the Win32 host only
#endif
#define DLLIMPORT __declspec(dllimport)
#define STDCALL __stdcall
#define API __declspec(dllexport)

typedef void (STDCALL *HostLogFn)(const uint16_t *plugin_id, const uint16_t *message);
#pragma pack(push,4)
typedef struct {
 uint32_t size, apiVersion;
 void *gameModule;
 const uint16_t *gameRoot,*executablePath,*pluginRoot,*configPath;
 HostLogFn log;
 void* configGetBool;
 void* configGetInt;
 void* configGetString;
} XenoPluginHostV1;
typedef struct {
 uint32_t size, requiredHostApi;
 uint16_t id[64],version[32],description[160];
 uint64_t exclusiveCapabilities;
} XenoPluginInfoV1;
#pragma pack(pop)

DLLIMPORT void* STDCALL CreateFileW(const uint16_t*,uint32_t,uint32_t,void*,uint32_t,uint32_t,void*);
DLLIMPORT int STDCALL ReadFile(void*,void*,uint32_t,uint32_t*,void*);
DLLIMPORT int STDCALL CloseHandle(void*);
DLLIMPORT uint32_t STDCALL GetModuleFileNameW(void*,uint16_t*,uint32_t);
DLLIMPORT void* STDCALL GetModuleHandleW(const uint16_t *);
DLLIMPORT uint32_t STDCALL GetTempPathW(uint32_t,uint16_t*);
DLLIMPORT int STDCALL WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
DLLIMPORT int STDCALL CryptAcquireContextW(uint32_t*,const uint16_t*,const uint16_t*,uint32_t,uint32_t);
DLLIMPORT int STDCALL CryptReleaseContext(uint32_t,uint32_t);
DLLIMPORT int STDCALL CryptCreateHash(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t*);
DLLIMPORT int STDCALL CryptHashData(uint32_t,const uint8_t*,uint32_t,uint32_t);
DLLIMPORT int STDCALL CryptGetHashParam(uint32_t,uint32_t,uint8_t*,uint32_t*,uint32_t);
DLLIMPORT int STDCALL CryptDestroyHash(uint32_t);

#define GENERIC_READ 0x80000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define PROV_RSA_AES 24u
#define CRYPT_VERIFYCONTEXT 0xF0000000u
#define CALG_SHA_256 0x800cu
#define HP_HASHVAL 2u
#define ERR_REVISION_MISMATCH 1306u
#define ERR_BAD_ARGUMENTS 87u
#define ERR_BAD_FORMAT 11u
#define DLL_PROCESS_ATTACH 1u

static const uint16_t k_id[]={'f','h','d','.','n','p','c','_','a','i',0};
static const uint16_t k_version[]={'2','.','1','.','3',0};
static const uint16_t k_description[]={'F','H','D',' ','N','P','C',' ','A','I',' ','2','.','1',' ','n','e','w',' ','E','X','E',' ','t','e','s','t',0};
static const uint16_t k_ok[]={'F','H','D','_','N','P','C','_','A','I',':',' ','E','X','E',' ','h','a','s','h',' ','O','K',0};
static const uint16_t k_no[]={'F','H','D','_','N','P','C','_','A','I',':',' ','d','i','s','a','b','l','e','d',' ','-',' ','E','X','E',' ','h','a','s','h',' ','n','o','t',' ','m','a','t','c','h','e','d',' ','(','n','o',' ','h','o','o','k','s',')',0};
static const uint8_t expected_sha256[32]={
0xe4,0x3a,0x4b,0x15,0x50,0x3c,0x70,0x5e,0xeb,0xe2,0xd8,0xce,0x57,0x78,0xc2,0xa2,
0xfb,0x09,0x0d,0xb5,0x15,0xa3,0xfd,0x86,0xd3,0x19,0xe5,0xc5,0xaa,0x69,0x2e,0xbf};
static uint32_t g_status=0;
static uint8_t g_read_buffer[32768];
static uint16_t g_exe_path[2048];
static uint16_t g_trace_path[1100];
static void copy_w(uint16_t *out,uint32_t cap,const uint16_t *in){uint32_t n=0;for(;n+1u<cap && in[n];n++)out[n]=in[n];out[n]=0;}
static int is_expected_sha(const uint8_t *hash){uint32_t i;uint8_t diff=0;for(i=0;i<32;i++)diff|=(uint8_t)(hash[i]^expected_sha256[i]);return diff==0;}
static int hash_executable(const uint16_t* path){
 uint32_t prov=0,hash=0,num=0,len=32;
 uint8_t out[32];
 void* file;
 int result=0,ok=0;
 if(!path||!path[0])return 0;
 file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(file==(void*)(intptr_t)-1)return 0;
 if(!CryptAcquireContextW(&prov,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))goto cleanup;
 if(!CryptCreateHash(prov,CALG_SHA_256,0,0,&hash))goto cleanup;
 do {
   if(!ReadFile(file,g_read_buffer,sizeof(g_read_buffer),&num,0))goto cleanup;
   if(num && !CryptHashData(hash,g_read_buffer,num,0))goto cleanup;
 } while(num);
 if(CryptGetHashParam(hash,HP_HASHVAL,out,&len,0)&&len==32){ok=1;result=is_expected_sha(out);}
cleanup:
 if(hash)CryptDestroyHash(hash);
 if(prov)CryptReleaseContext(prov,0);
 CloseHandle(file);
 return ok ? result : 0;
}
API int32_t STDCALL XenoPlugin_Query(XenoPluginInfoV1* info){
 if(!info||info->size<offsetof(XenoPluginInfoV1,exclusiveCapabilities))return 0;
 info->requiredHostApi=1;
 copy_w(info->id,64,k_id);
 copy_w(info->version,32,k_version);
 copy_w(info->description,160,k_description);
 if(info->size>=sizeof(XenoPluginInfoV1))info->exclusiveCapabilities=0;
 return 1;
}
API uint32_t STDCALL XenoPlugin_Initialize(const XenoPluginHostV1* host){
 if(!host||host->size<offsetof(XenoPluginHostV1,configGetBool)||host->apiVersion<1||!host->gameModule||!host->executablePath)
  return ERR_BAD_ARGUMENTS;
 if(!hash_executable(host->executablePath)){
  g_status=0;
  if(host->log)host->log(k_id,k_no);
  return ERR_REVISION_MISMATCH;
 }
 g_status=1;
 if(host->log)host->log(k_id,k_ok);
 return 0;
}
API uint32_t FHD_ProbeStatus(void){return g_status;}


/* Engine-native ScriptLibs imports. No XenoNativeLoader. Exact hash required. */
extern void FHD_BridgeNewGame(void);

/* Write a tiny proof-of-life log without touching any game object or UI.
   This uses Windows TEMP rather than protected Program Files locations. */
static void write_probe_log(const char *line){
  static const uint16_t suffix[]={ 'F','H','D','_','N','P','C','_','A','I','_','p','r','o','b','e','.','l','o','g',0};
  uint32_t i=0,j=0,written=0;
  char ascii[128];void* h;
  uint32_t n=GetTempPathW(1024,g_trace_path);
  if(!n||n+sizeof(suffix)/2>=1100)return;
  for(i=0;i<sizeof(suffix)/2;i++)g_trace_path[n+i]=suffix[i];
  h=CreateFileW(g_trace_path,4u,FILE_SHARE_READ,0,4u,FILE_ATTRIBUTE_NORMAL,0);
  if(h==(void*)(intptr_t)-1)return;
  for(j=0;line[j]&&j<sizeof(ascii)-2;j++)ascii[j]=line[j];
  ascii[j++]='\r';ascii[j++]='\n';
  WriteFile(h,ascii,j,&written,0);
  CloseHandle(h);
}

API int32_t FHDNPC_Init(void){
  uint32_t n=GetModuleFileNameW(0,g_exe_path,2048);
  if(!n||n>=2048){g_status=0;write_probe_log("Init: cannot locate EXE");return 0;}
  g_status=(uint32_t)hash_executable(g_exe_path);
  if(g_status){
    FHDNPC_DialogueSettingsLoad();
    write_probe_log(FHD_SettingsArm()?"NPC AI 2.1.1 space settings tab armed":"NPC AI 2.1.1 space settings tab unavailable");
  }
  write_probe_log(g_status?"Init: expected 2026 EXE SHA256 OK":"Init: EXE SHA256 not allowed");
  return (int32_t)g_status;
}
extern int32_t FHDNPC_ArmObserver(void);
extern int32_t FHDNPC_ArmSaveObserver(void);
extern int32_t FHDNPC_ArmSocial(void);
extern int32_t FHDNPC_ArmTrade(void);
extern int32_t FHDNPC_ArmGrievance(void);
extern uint32_t FHDNPC_GrievanceCapabilities(void);
extern void FHDNPC_ResetSaveStage(void);
extern int32_t FHDNPC_RestoreLoadedSnapshot(void);
extern void FHDNPC_ResetCounters(void);
extern int32_t FHDNPC_SaveProbe(void);
extern void FHDNPC_LogMemoryAudit(const char *);
static void bind_loaded_player(void){
  const uint8_t *base=GetModuleHandleW(0);
  typedef const uint8_t *(*GetPlayerFn)(void);
  if(!base)return;
  const uint8_t *player=((GetPlayerFn)(base+0x347300u))();
  if(player&&player[0x10u]==1u&&*(const uint32_t *)(player+4u))
    FHD_BindLegacyPlayer(*(const uint32_t *)(player+4u),*(const uint32_t *)(player+0xe0u));
}
API int32_t FHDNPC_OnStart(void){
  int m;
  if(!g_status)return 0;
  FHD_BridgeNewGame();FHDNPC_ResetCounters();FHDNPC_NewsReset();FHDNPC_ResetSaveStage();
  if(FHDNPC_ArmObserver()!=1){write_probe_log("OnStart: VMT observer NOT armed");return 0;}
  write_probe_log(FHDNPC_ArmTrade()==1?"NPC AI 1.8 trade route armed":"NPC AI 1.8 trade route unavailable");
  write_probe_log(FHDNPC_ArmDuties()==1?"NPC AI 2.0 duties armed":"NPC AI 2.0 duties unavailable");
  {int social=FHDNPC_ArmSocial();
    write_probe_log(social==1?"OnStart: AI 1.8 social/transport/military tactical hooks armed":"OnStart: AI 1.8 social hooks NOT armed; vanilla preserved");}
  {int grievance=FHDNPC_ArmGrievance();
    write_probe_log(grievance==1?"NPC AI 1.8 grievance, loot, reputation and bounty hooks armed":"NPC AI 1.8 grievance hooks NOT armed; vanilla preserved");}
  {uint32_t loot=FHDNPC_GrievanceCapabilities();
   write_probe_log((loot&1u)?"NPC AI 2.1.3 missile loot armed":"NPC AI 2.1.3 missile loot unavailable");
   write_probe_log((loot&2u)?"NPC AI 2.1.3 relation levels armed":"NPC AI 2.1.3 relation levels unavailable");}
  m=FHDNPC_ArmSaveObserver();
  write_probe_log(m==1?"OnStart: AI 1.8 NPC policy + SAV worker observer armed":"OnStart: AI 1.8 policy; SAV worker observer DISABLED");
  return 1;
}
API int32_t FHDNPC_OnLoad(void){
  int32_t m,r;
  if(!g_status)return 0;
  FHD_BridgeNewGame();FHDNPC_ResetCounters();FHDNPC_NewsReset();FHDNPC_ResetSaveStage();
  if(FHDNPC_ArmObserver()!=1){write_probe_log("OnLoad: VMT observer NOT armed");return 0;}
  write_probe_log(FHDNPC_ArmTrade()==1?"NPC AI 1.8 trade route armed":"NPC AI 1.8 trade route unavailable");
  write_probe_log(FHDNPC_ArmDuties()==1?"NPC AI 2.0 duties armed":"NPC AI 2.0 duties unavailable");
  {int social=FHDNPC_ArmSocial();
    write_probe_log(social==1?"OnLoad: AI 1.8 social hooks armed":"OnLoad: AI 1.8 social hooks NOT armed; vanilla preserved");}
  {int grievance=FHDNPC_ArmGrievance();
    write_probe_log(grievance==1?"NPC AI 1.8 grievance, loot, reputation and bounty hooks armed":"NPC AI 1.8 grievance hooks NOT armed; vanilla preserved");}
  {uint32_t loot=FHDNPC_GrievanceCapabilities();
   write_probe_log((loot&1u)?"NPC AI 2.1.3 missile loot armed":"NPC AI 2.1.3 missile loot unavailable");
   write_probe_log((loot&2u)?"NPC AI 2.1.3 relation levels armed":"NPC AI 2.1.3 relation levels unavailable");}
  m=FHDNPC_ArmSaveObserver();
  r=m==1?FHDNPC_RestoreLoadedSnapshot():0;
  if(r==1){write_probe_log("OnLoad: AI 1.8 SAV memory RESTORED and paired");
    bind_loaded_player();
    FHDNPC_LogMemoryAudit("RESTORED");}
  else write_probe_log("OnLoad: AI 1.8 SAV memory unavailable/mismatched; reset");
  return 1;
}
API int32_t FHDNPC_Status(void){return (int32_t)g_status;}
API int32_t FHDNPC_OnSaveProbe(void){if(!g_status)return 0;return FHDNPC_SaveProbe();}
