/* Persist the native settings screen choice beside the DLL. */
#include <stdint.h>
#include "fhd_dialogue.h"
#if defined(_WIN32)
#define IMP __declspec(dllimport)
#define ST __stdcall
IMP void *ST GetModuleHandleW(const uint16_t*);
IMP uint32_t ST GetModuleFileNameW(void*,uint16_t*,uint32_t);
IMP void *ST CreateFileW(const uint16_t*,uint32_t,uint32_t,void*,uint32_t,uint32_t,void*);
IMP int ST ReadFile(void*,void*,uint32_t,uint32_t*,void*);
IMP int ST WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
IMP int ST CloseHandle(void*);
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define CREATE_ALWAYS 2u
#define NORMAL 0x80u
static uint16_t filename[1024];
static int filename_ready;
static int get_path(void){
 static const uint16_t dll[]={ 'F','H','D','_','N','P','C','_','A','I','.','d','l','l',0};
 static const uint16_t suffix[]={ 'F','H','D','N','P','C','_','d','i','a','l','o','g','u','e','.','m','o','d','e',0};
 uint32_t n,i,last=0;void *h;
 if(filename_ready)return 1;
 h=GetModuleHandleW(dll);if(!h)return 0;
 n=GetModuleFileNameW(h,filename,1024);if(!n||n>=1024)return 0;
 for(i=0;i<n;i++)if(filename[i]=='\\'||filename[i]=='/')last=i+1u;
 if(!last||last+sizeof(suffix)/2u>=1024u)return 0;
 for(i=0;i<sizeof(suffix)/2u;i++)filename[last+i]=suffix[i];
 filename_ready=1;return 1;
}
void FHDNPC_DialogueSettingsLoad(void){
 void*h;char text[4]={0};uint32_t count=0;
 if(!get_path())return;
 h=CreateFileW(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,NORMAL,0);
 if(h==(void*)(intptr_t)-1)return;
 {int valid=ReadFile(h,text,3u,&count,0)&&count&&text[0]>='0'&&text[0]<='2';
  CloseHandle(h);
  if(valid)(void)FHDNPC_DialogueModeSet((uint32_t)(text[0]-'0'));
 }
}
void FHDNPC_DialogueSettingsSave(uint32_t mode){
 void*h;char text[2];uint32_t count=0;
 if(mode>2u||!get_path())return;
 text[0]=(char)('0'+mode);text[1]='\n';
 h=CreateFileW(filename,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,NORMAL,0);
 if(h==(void*)(intptr_t)-1)return;
 (void)WriteFile(h,text,2u,&count,0);CloseHandle(h);
}
#else
void FHDNPC_DialogueSettingsLoad(void){}
void FHDNPC_DialogueSettingsSave(uint32_t mode){(void)mode;}
#endif
