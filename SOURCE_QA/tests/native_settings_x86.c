/* The host models the native space groups and the OK handler. */
#define FHD_SETTINGS_TEST
#include "../src/fhd_dialogue_ui.c"

static uint8_t game[0x160000],form[0x140],button[0x1f4];
static uint32_t dialogue_mode=1,selected,labels_built,choices_built,settings_writes;
static uint16_t caption_copy[16];
static uint16_t strings[8][32];
static unsigned allocations;
static void *fail_patch;
static unsigned get32(const void *p,unsigned off){return *(const unsigned *)((const uint8_t *)p+off);}
static void put32(void *p,unsigned off,unsigned value){*(unsigned *)((uint8_t *)p+off)=value;}
static int equal(const uint16_t *a,const uint16_t *b){
  while(*a&&*a==*b){a++;b++;}
  return *a==*b;
}
#define CHECK(expr) do {if(!(expr))return __LINE__;} while(0)
void *GetModuleHandleW(const uint16_t *unused){(void)unused;return game;}
void *GetCurrentProcess(void){return 0;}
int VirtualProtect(void *p,uint32_t n,uint32_t mode,uint32_t *old){
  (void)n;*old=0x20;
  if(mode==0x40&&p==fail_patch)return 0;
  return 1;
}
int FlushInstructionCache(void *p,const void *a,uint32_t n){(void)p;(void)a;(void)n;return 1;}
uint16_t *SysAllocStringLen(const uint16_t *p,uint32_t n){
  if(allocations>=8||n>=32)return 0;
  uint16_t *out=strings[allocations++];
  for(unsigned i=0;i<n;i++)out[i]=p[i];
  out[n]=0;return out;
}
void SysFreeString(uint16_t *p){(void)p;}
uint32_t FHDNPC_DialogueModeGet(void){return dialogue_mode;}
int32_t FHDNPC_DialogueModeSet(uint32_t mode){settings_writes++;dialogue_mode=mode;return 1;}
void FHD_SettingsBuildStub(void){}
void FHD_SettingsApplyStub(void){}
void FHD_SettingsShowTabStub(void){}
void FHD_SettingsLabel(void *entry,void *self,const uint16_t *key,const uint16_t *title){
  if(entry==game+0x13a6b4&&self==form&&get32(self,BUILD_GROUP)==3&&
     equal(key,u"FHDNPCDialogueMode")&&equal(title,u"Реплики НПЦ"))labels_built++;
}
void FHD_SettingsChoice(void *entry,void *self,uint32_t value,const uint16_t *title,uint32_t active){
  const uint16_t *names[]={u"Только штатные",u"События игрока",u"Местный эфир"};
  if(entry==game+0x13a93c&&self==form&&get32(self,BUILD_GROUP)==3&&value<3&&
     active==(value==dialogue_mode)&&equal(names[value],title))choices_built++;
}
static void *REG get_control(void *self,const uint16_t *key){
  return self==form&&equal(key,u"ButGroup3")?button:0;
}
static void REG set_caption(void *control,const uint16_t *caption){
  if(control==button)for(unsigned i=0;i<16;i++){
    caption_copy[i]=caption[i];
    if(!caption[i])break;
  }
}
static int REG has_option(void *self,const uint16_t *key){
  return self==form&&get32(self,ACTIVE_GROUP)==3&&equal(key,u"FHDNPCDialogueMode");
}
static int REG read_option(void *self,const uint16_t *key){return has_option(self,key)?(int)selected:-1;}
static void copy(uint8_t *out,const uint8_t *in,unsigned n){
  for(unsigned i=0;i<n;i++)out[i]=in[i];
}
static void branch(unsigned rva,const void *fn){
  game[rva]=0xe9;
  put32(game,rva+1,(uintptr_t)fn-(uintptr_t)(game+rva+5));
}
static int run(void){
  const uint8_t label[]={0x55,0x8b,0xec,0x83,0xc4,0xdc},
                choice[]={0x55,0x8b,0xec,0x83,0xc4,0xc8},
                reader[]={0x55,0x8b,0xec,0x33,0xc9,0x51},
                control[]={0x55,0x8b,0xec,0x6a,0,0x6a,0,0x53},
                caption[]={0x53,0x56,0x57,0x83,0xc4,0xf8,0x8b,0xfa};
  copy(game+0x13a6b4,label,sizeof(label));copy(game+0x13a93c,choice,sizeof(choice));
  copy(game+0x13b868,reader,sizeof(reader));copy(game+0x13b99c,reader,sizeof(reader));
  copy(game+GET_CONTROL,control,sizeof(control));copy(game+SET_CAPTION,caption,sizeof(caption));
  copy(game+GROUP3_NAME,group3_signature,sizeof(group3_signature));
  for(unsigned i=0;i<SITE_COUNT;i++)copy(game+sites[i].rva,sites[i].bytes,sites[i].size);
  fail_patch=game+sites[2].rva;
  CHECK(!FHD_SettingsArm());
  for(unsigned i=0;i<SITE_COUNT;i++)CHECK(same(game+sites[i].rva,sites[i].bytes,sites[i].size));
  fail_patch=0;
  CHECK(FHD_SettingsArm()&&game[sites[3].rva]==0xeb);
  branch(GET_CONTROL,get_control);branch(SET_CAPTION,set_caption);
  branch(0x13b868,has_option);branch(0x13b99c,read_option);

  put32(form,GRAPHICS_TOP,244);put32(form,MUSIC_TOP,284);put32(form,NPC_TOP,345);
  CHECK(!FHD_SettingsShowTab(form));
  FHD_SettingsBuild(form);
  CHECK(!labels_built&&!FHD_SettingsShowTab(form));
  put32(form,NPC_PANEL,1);
  FHD_SettingsBuild(form);
  CHECK(FHD_SettingsShowTab(form)&&labels_built==1&&choices_built==3);
  CHECK(equal(caption_copy,u"ИИ НПЦ")&&get32(form,NPC_TOP)==324&&get32(form,BUILD_GROUP)==3);
  CHECK(dialogue_mode==1&&!settings_writes);
  put32(form,SETTINGS_MODE,1);
  CHECK(!FHD_SettingsShowTab(form));
  put32(form,SETTINGS_MODE,0);
  put32(form,ACTIVE_GROUP,2);
  selected=2;allocations=0;
  FHD_SettingsApply(form);
  CHECK(dialogue_mode==2&&settings_writes==1&&get32(form,ACTIVE_GROUP)==2);
  FHD_SettingsApply(form);
  CHECK(settings_writes==1);
  put32(form,NPC_PANEL,2);
  selected=0;
  CHECK(!FHD_SettingsShowTab(form));
  FHD_SettingsApply(form);
  CHECK(dialogue_mode==2&&settings_writes==1);
  FHD_SettingsDisarm();
  for(unsigned i=0;i<SITE_COUNT;i++)CHECK(same(game+sites[i].rva,sites[i].bytes,sites[i].size));
  return 0;
}
void _start(void){
  int result=run();
  __asm__ volatile ("int $0x80"::"a"(1),"b"(result):"memory");
  __builtin_unreachable();
}
