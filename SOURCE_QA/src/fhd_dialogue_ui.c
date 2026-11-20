/* Use the existing fourth space-settings button and panel for NPC options. */
#include "fhd_dialogue.h"
#include <stdint.h>
#if defined(FHD_SETTINGS_TEST) && defined(__i386__)
#define IMP
#define ST
#else
#define IMP __declspec(dllimport)
#define ST __stdcall
#endif
#define REG __attribute__((regparm(3)))
IMP void *ST GetModuleHandleW(const uint16_t *);
IMP void *ST GetCurrentProcess(void);
IMP int ST VirtualProtect(void *,uint32_t,uint32_t,uint32_t *);
IMP int ST FlushInstructionCache(void *,const void *,uint32_t);
IMP uint16_t *ST SysAllocStringLen(const uint16_t *,uint32_t);
IMP void ST SysFreeString(uint16_t *);
extern void FHD_SettingsBuildStub(void);
extern void FHD_SettingsApplyStub(void);
extern void FHD_SettingsShowTabStub(void);
extern void FHD_SettingsChoice(void *entry,void *form,uint32_t value,
                               const uint16_t *caption,uint32_t selected);
extern void FHD_SettingsLabel(void *entry,void *form,const uint16_t *key,const uint16_t *title);
typedef int (REG *ReadOption)(void *,const uint16_t *);
typedef void *(REG *GetControl)(void *,const uint16_t *);
typedef void (REG *SetCaption)(void *,const uint16_t *);
enum { ACTIVE_GROUP=0xd0, BUILD_GROUP=0xd4, NPC_GROUP=3, NPC_PANEL=0xe8, SETTINGS_MODE=0x10c,
       GRAPHICS_TOP=0x128, MUSIC_TOP=0x12c, NPC_TOP=0x130,
       GET_CONTROL=0x0c5940, SET_CAPTION=0x0acadc };
static const void *built_form,*built_panel;
static uint8_t *image;
static unsigned armed;
static const uint16_t option[]=u"FHDNPCDialogueMode";
static const uint16_t heading[]=u"Реплики НПЦ";
static const uint16_t tab_title[]=u"ИИ НПЦ";
static const uint16_t *const labels[]={u"Только штатные",u"События игрока",u"Местный эфир"};
static const struct {unsigned rva,size;uint8_t bytes[10];void (*stub)(void);} sites[]={
  {0x13720b,10,{0xc7,0x83,0xd4,0,0,0,3,0,0,0},FHD_SettingsBuildStub},
  {0x13ffd0,8,{0x33,0xc0,0x89,0x83,0xd0,0,0,0},FHD_SettingsApplyStub},
  {0x13f1ad,9,{0x33,0xd2,0x8b,0xc6,0x8b,0x08,0xff,0x51,0x28},FHD_SettingsShowTabStub},
  /* The old mod warning belonged to the unused fourth space group. */
  {0x13f0b8,2,{0x75,0x17},0}
};
enum {SITE_COUNT=sizeof(sites)/sizeof(sites[0])};
static uint8_t installed[SITE_COUNT][10];
static const uint8_t group3_signature[]={'B',0,'u',0,'t',0,'G',0,'r',0,'o',0,'u',0,'p',0,'3',0,0,0};
#define GROUP3_NAME 0x13a688u
static int same(const uint8_t *a,const uint8_t *b,unsigned n){
  for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;
  return 1;
}
static uint16_t *wide(const uint16_t *s){
  unsigned n=0;
  while(s[n])n++;
  return SysAllocStringLen(s,n);
}
static unsigned group_top(uint8_t *form,unsigned offset){
  return *(const unsigned *)(form+offset);
}
static int has_tab(const uint8_t *form){
  return form==built_form&&built_panel&&*(void *const *)(form+NPC_PANEL)==built_panel;
}
void FHD_SettingsBuild(void *form){
  uint8_t *self=form;
  built_form=built_panel=0;
  void *panel=*(void **)(self+NPC_PANEL);
  if(!panel)return;
  unsigned graphics=group_top(self,GRAPHICS_TOP),music=group_top(self,MUSIC_TOP);
  unsigned top=music>graphics?music+(music-graphics):group_top(self,NPC_TOP);
  uint16_t *key=wide(option),*title=wide(heading),*tab=wide(tab_title);
  uint16_t *choices[3]={wide(labels[0]),wide(labels[1]),wide(labels[2])};
  if(key&&title&&tab&&choices[0]&&choices[1]&&choices[2]){
    void *button=((GetControl)(image+GET_CONTROL))(form,(const uint16_t *)(image+GROUP3_NAME));
    if(button){
      ((SetCaption)(image+SET_CAPTION))(button,tab);
      *(unsigned *)(self+NPC_TOP)=top;
      *(unsigned *)(self+BUILD_GROUP)=NPC_GROUP;
      FHD_SettingsLabel(image+0x13a6b4,form,key,title);
      for(unsigned i=0;i<3;i++)
        FHD_SettingsChoice(image+0x13a93c,form,i,choices[i],i==FHDNPC_DialogueModeGet());
      built_form=form;
      built_panel=panel;
    }
  }
  SysFreeString(tab);SysFreeString(key);SysFreeString(title);
  for(unsigned i=0;i<3;i++)SysFreeString(choices[i]);
}
uint32_t FHD_SettingsShowTab(void *form){
  /* Called only from the space branch of RefreshModeUi. */
  return has_tab(form)&&!group_top(form,SETTINGS_MODE);
}
void FHD_SettingsApply(void *form){
  uint8_t *self=form;
  if(!has_tab(self))return;
  uint16_t *key=wide(option);
  if(!key)return;
  unsigned previous=*(unsigned *)(self+ACTIVE_GROUP);
  *(unsigned *)(self+ACTIVE_GROUP)=NPC_GROUP;
  if(((ReadOption)(image+0x13b868))(form,key)){
    int value=((ReadOption)(image+0x13b99c))(form,key);
    if(value>=0&&value<=2&&(unsigned)value!=FHDNPC_DialogueModeGet())
      (void)FHDNPC_DialogueModeSet((unsigned)value);
  }
  *(unsigned *)(self+ACTIVE_GROUP)=previous;
  SysFreeString(key);
}
static int replace(unsigned i,const uint8_t *expect,const uint8_t *after){
  uint32_t old,unused;
  uint8_t *at=image+sites[i].rva;
  if(!same(at,expect,sites[i].size)||!VirtualProtect(at,sites[i].size,0x40,&old))return 0;
  if(!same(at,expect,sites[i].size)){VirtualProtect(at,sites[i].size,old,&unused);return 0;}
  for(unsigned j=0;j<sites[i].size;j++)at[j]=after[j];
  FlushInstructionCache(GetCurrentProcess(),at,sites[i].size);
  VirtualProtect(at,sites[i].size,old,&unused);return 1;
}
int FHD_SettingsArm(void){
  if(armed)return 1;
  image=GetModuleHandleW(0);
  if(!image)return 0;
  static const uint8_t label[]={0x55,0x8b,0xec,0x83,0xc4,0xdc},
                       choice[]={0x55,0x8b,0xec,0x83,0xc4,0xc8},
                       reader[]={0x55,0x8b,0xec,0x33,0xc9,0x51},
                       control[]={0x55,0x8b,0xec,0x6a,0,0x6a,0,0x53},
                       caption[]={0x53,0x56,0x57,0x83,0xc4,0xf8,0x8b,0xfa};
  if(!same(image+0x13a6b4,label,sizeof(label))||!same(image+0x13a93c,choice,sizeof(choice))||
     !same(image+0x13b868,reader,sizeof(reader))||!same(image+0x13b99c,reader,sizeof(reader))||
     !same(image+GET_CONTROL,control,sizeof(control))||!same(image+SET_CAPTION,caption,sizeof(caption))||
     !same(image+GROUP3_NAME,group3_signature,sizeof(group3_signature)))return 0;
  for(unsigned i=0;i<SITE_COUNT;i++)
    if(!same(image+sites[i].rva,sites[i].bytes,sites[i].size))return 0;
  for(unsigned i=0;i<SITE_COUNT;i++){
    if(sites[i].stub){
      uint32_t displacement=(uint32_t)((uintptr_t)sites[i].stub-(uintptr_t)(image+sites[i].rva+5));
      installed[i][0]=0xe8;
      for(unsigned j=0;j<4;j++)installed[i][j+1]=(uint8_t)(displacement>>(8*j));
      for(unsigned j=5;j<sites[i].size;j++)installed[i][j]=0x90;
    }else{
      installed[i][0]=0xeb; /* Jump past the unused group warning. */
      installed[i][1]=sites[i].bytes[1];
    }
    if(!replace(i,sites[i].bytes,installed[i])){
      while(i){i--;replace(i,installed[i],sites[i].bytes);}
      return 0;
    }
  }
  armed=1;return 1;
}
void FHD_SettingsDisarm(void){
  if(!armed)return;
  for(unsigned i=SITE_COUNT;i>0;i--)replace(i-1,installed[i-1],sites[i-1].bytes);
  built_form=built_panel=0;
  armed=0;
}
