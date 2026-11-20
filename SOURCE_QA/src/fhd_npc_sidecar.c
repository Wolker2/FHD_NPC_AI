#include "fhd_npc_bridge.h"

/* Native, atomic sidecar. No SAV byte mutation and no external launcher.
 * Requires game adapter to supply the completed SAV filename + unique galaxy key.
 * On Linux, filesystem tests use ASCII paths. Windows uses full UTF-16. */
#pragma pack(push,1)
typedef struct SlotHeader {
  uint8_t magic[8];
  uint32_t version, data_size, checksum;
  uint64_t galaxy_key;
} SlotHeader;
#pragma pack(pop)
#define SLOT_CAPACITY 4194304u
#define PATH_CAPACITY 2048u
static uint8_t g_buffer[SLOT_CAPACITY];
static uint16_t g_path[PATH_CAPACITY],g_temp_path[PATH_CAPACITY];
static uint16_t g_pending_path[PATH_CAPACITY],g_pending_temp[PATH_CAPACITY];
static uint16_t g_move_src[PATH_CAPACITY],g_move_dst[PATH_CAPACITY];
static uint64_t g_pending_key;
static uint32_t g_pending;

#if defined(_WIN32)
typedef void* HFILE;
#define DLLIMPORT __declspec(dllimport)
DLLIMPORT HFILE __stdcall CreateFileW(const uint16_t*,uint32_t,uint32_t,void*,uint32_t,uint32_t,void*);
DLLIMPORT int __stdcall ReadFile(HFILE,void*,uint32_t,uint32_t*,void*);
DLLIMPORT int __stdcall WriteFile(HFILE,const void*,uint32_t,uint32_t*,void*);
DLLIMPORT int __stdcall FlushFileBuffers(HFILE);
DLLIMPORT int __stdcall GetFileSizeEx(HFILE,int64_t*);
DLLIMPORT int __stdcall CloseHandle(HFILE);
DLLIMPORT int __stdcall MoveFileExW(const uint16_t*,const uint16_t*,uint32_t);
DLLIMPORT int __stdcall DeleteFileW(const uint16_t*);
#define HANDLE_INVALID ((HFILE)(intptr_t)-1)
#define OPEN_EXISTING 3u
#define CREATE_ALWAYS 2u
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define MOVEFILE_REPLACE_EXISTING 1u
#define MOVEFILE_WRITE_THROUGH 8u
static int winwrite(const uint16_t*path,const void*data,uint32_t len){
  HFILE h=CreateFileW(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  uint32_t offset=0,num=0;int ok=1;
  if(h==HANDLE_INVALID)return 0;
  while(offset<len){if(!WriteFile(h,(const uint8_t*)data+offset,len-offset,&num,0)||!num){ok=0;break;}offset+=num;}
  if(ok&&!FlushFileBuffers(h))ok=0;
  if(!CloseHandle(h))ok=0;
  return ok;
}
static int winread(const uint16_t*path,void*data,uint32_t cap,uint32_t*out){
  HFILE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  int64_t size=0;uint32_t offset=0,num=0;int ok=1;
  if(h==HANDLE_INVALID)return 0;
  if(!GetFileSizeEx(h,&size)||size<=0||size>cap)ok=0;
  while(ok&&offset<(uint32_t)size){
    if(!ReadFile(h,(uint8_t*)data+offset,(uint32_t)size-offset,&num,0)||!num){ok=0;break;}
    offset+=num;
  }
  CloseHandle(h);
  if(ok)*out=offset;
  return ok;
}
#define WRITEFILE winwrite
#define READFILE winread
#define MOVEFILE(src,dst) MoveFileExW(src,dst,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)
#define DELETEFILE(path) DeleteFileW(path)
#else
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
static int cpath(const uint16_t *w,char*dst){
  uint32_t i;for(i=0;i<PATH_CAPACITY;i++){
    if(w[i]>127u)return 0;
    dst[i]=(char)w[i];if(!w[i])return 1;
  }return 0;
}
static int linuxwrite(const uint16_t*path,const void*data,uint32_t len){
  char p[PATH_CAPACITY];FILE*f;int ok;
  if(!cpath(path,p))return 0;
  f=fopen(p,"wb");if(!f)return 0;
  ok=(fwrite(data,1,len,f)==len);
  if(fflush(f)!=0)ok=0;
  if(fsync(fileno(f))!=0)ok=0;
  if(fclose(f)!=0)ok=0;
  return ok;
}
static int linuxread(const uint16_t*path,void*data,uint32_t cap,uint32_t*out){
  char p[PATH_CAPACITY];FILE*f;long len;int ok;
  if(!cpath(path,p))return 0;
  f=fopen(p,"rb");if(!f)return 0;
  if(fseek(f,0,SEEK_END)||((len=ftell(f))<=0)||((uint64_t)len>cap)||fseek(f,0,SEEK_SET)){
    fclose(f);return 0;
  }
  ok=fread(data,1,(uint32_t)len,f)==(uint32_t)len;
  fclose(f);if(ok)*out=(uint32_t)len;return ok;
}
static int linuxmove(const uint16_t*from,const uint16_t*to){
  char a[PATH_CAPACITY],b[PATH_CAPACITY];
  return cpath(from,a)&&cpath(to,b)&&rename(a,b)==0;
}
static void linuxdelete(const uint16_t*path){char a[PATH_CAPACITY];if(cpath(path,a))remove(a);}
#define WRITEFILE linuxwrite
#define READFILE linuxread
#define MOVEFILE linuxmove
#define DELETEFILE linuxdelete
#endif

static uint32_t checksum(const uint8_t*buf,uint32_t count){
  uint32_t h=2166136261u,i;
  for(i=0;i<count;i++)h=(h^buf[i])*16777619u;
  return h;
}
static void cp(void*dst,const void*src,uint32_t n){
  uint8_t*d=(uint8_t*)dst;const uint8_t*s=(const uint8_t*)src;
  while(n--)*d++=*s++;
}
static int slot_path(const uint16_t*src){
  static const char suffix[]=".fhdnpc";
  static const char temp[]=".tmp";
  uint32_t n=0,i;
  if(!src)return 0;
  while(n<PATH_CAPACITY-20u&&src[n]){
    if(src[n]<32u)return 0;
    g_path[n]=src[n];n++;
  }
  if(n<3u||n>=PATH_CAPACITY-20u)return 0;
  for(i=0;i<sizeof(suffix)-1;i++)g_path[n++]=(uint16_t)suffix[i];
  g_path[n]=0;
  for(i=0;i<=n;i++)g_temp_path[i]=g_path[i];
  for(i=0;i<sizeof(temp)-1;i++)g_temp_path[n+i]=(uint16_t)temp[i];
  g_temp_path[n+i]=0;
  return 1;
}

/* Capture state when the game queues its asynchronous disk write. The final
   rename must be called only by the writer, AFTER successful SAV replacement.
   One in-flight game save is supported, matching TSaver.QueueSave serialization.
   An unsuccessful save must never overwrite a valid older memory sidecar. */
static int eq_path(const uint16_t *a,const uint16_t *b){
  uint32_t i;for(i=0;i<PATH_CAPACITY;i++){
    if(a[i]!=b[i])return 0;if(!a[i])return 1;
  }return 0;
}
static void copy_path(uint16_t* dst,const uint16_t* src){
  uint32_t i=0;do{dst[i]=src[i];}while(src[i++]&&i<PATH_CAPACITY);
}
void FHD_CALL FHD_BridgeAbortSlot(void){
  if(g_pending)DELETEFILE(g_pending_temp);
  g_pending=0;g_pending_key=0;
}
int32_t FHD_CALL FHD_BridgePrepareSlot(const uint16_t *path,uint64_t key){
  SlotHeader h;int32_t size;
  FHD_BridgeAbortSlot();
  if(!key||!slot_path(path))return -1;
  size=FHD_Snapshot(0,0);
  if(size<0||(uint32_t)size+sizeof(h)>SLOT_CAPACITY)return -2;
  if(FHD_Snapshot(g_buffer+sizeof(h),SLOT_CAPACITY-(uint32_t)sizeof(h))!=size)return -3;
  h.magic[0]='F';h.magic[1]='H';h.magic[2]='D';h.magic[3]='S';
  h.magic[4]='A';h.magic[5]='V';h.magic[6]='0';h.magic[7]='4';
  h.version=4u;h.data_size=(uint32_t)size;h.galaxy_key=key;
  h.checksum=checksum(g_buffer+sizeof(h),(uint32_t)size);
  cp(g_buffer,&h,(uint32_t)sizeof(h));
  if(!WRITEFILE(g_temp_path,g_buffer,(uint32_t)size+(uint32_t)sizeof(h))){
    DELETEFILE(g_temp_path);return -4;
  }
  copy_path(g_pending_path,g_path);
  copy_path(g_pending_temp,g_temp_path);
  g_pending_key=key;g_pending=1;
  return 1;
}
int32_t FHD_CALL FHD_BridgeCommitSlot(const uint16_t *path,uint64_t key){
  if(!g_pending||!key||!slot_path(path)||key!=g_pending_key||
     !eq_path(g_path,g_pending_path))return 0;
  if(!MOVEFILE(g_pending_temp,g_pending_path)){
    FHD_BridgeAbortSlot();return -5;
  }
  g_pending=0;g_pending_key=0;
  return 1;
}


int32_t FHD_CALL FHD_BridgeMoveSlot(const uint16_t *from,const uint16_t *to){
  if(!slot_path(from))return -1;
  copy_path(g_move_src,g_path);
  if(!slot_path(to))return -1;
  copy_path(g_move_dst,g_path);
  if(eq_path(g_move_src,g_move_dst))return 1;
  /* Moving the SAV can leave a stale sidecar at the destination. If the source
     has no sidecar, delete the destination's old one to fail closed. */
  if(!MOVEFILE(g_move_src,g_move_dst)){
    DELETEFILE(g_move_dst);
    return 0;
  }
  return 1;
}

int32_t FHD_CALL FHD_BridgeSaveSlot(const uint16_t*path,uint64_t key){
  SlotHeader header;int32_t size;
  if(!key||!slot_path(path))return -1;
  size=FHD_Snapshot(0,0);
  if(size<0||(uint32_t)size+sizeof(header)>SLOT_CAPACITY)return -2;
  if(FHD_Snapshot(g_buffer+sizeof(header),SLOT_CAPACITY-(uint32_t)sizeof(header))!=size)return -3;
  header.magic[0]='F';header.magic[1]='H';header.magic[2]='D';header.magic[3]='S';
  header.magic[4]='A';header.magic[5]='V';header.magic[6]='0';header.magic[7]='4';
  header.version=4u;header.data_size=(uint32_t)size;
  header.galaxy_key=key;header.checksum=checksum(g_buffer+sizeof(header),(uint32_t)size);
  cp(g_buffer,&header,(uint32_t)sizeof(header));
  if(!WRITEFILE(g_temp_path,g_buffer,(uint32_t)size+(uint32_t)sizeof(header))){
    DELETEFILE(g_temp_path);return -4;
  }
  if(!MOVEFILE(g_temp_path,g_path)){DELETEFILE(g_temp_path);return -5;}
  return 1;
}
int32_t FHD_CALL FHD_BridgeLoadSlot(const uint16_t*path,uint64_t key){
  SlotHeader h;uint32_t n=0;int32_t result;
  /* Failed/unpaired slot must NEVER inherit another saved game's relationships. */
  FHD_BridgeAbortSlot();
  FHD_Reset();
  if(!key||!slot_path(path))return -1;
  if(!READFILE(g_path,g_buffer,SLOT_CAPACITY,&n))return -2;
  if(n<sizeof(h))return -3;
  cp(&h,g_buffer,(uint32_t)sizeof(h));
  if(h.magic[0]!='F'||h.magic[1]!='H'||h.magic[2]!='D'||h.magic[3]!='S'||
     h.magic[4]!='A'||h.magic[5]!='V'||h.magic[6]!='0'||h.magic[7]!='4'||
     h.version!=4u||h.galaxy_key!=key||
     h.data_size!=n-sizeof(h))return -4;
  if(checksum(g_buffer+sizeof(h),h.data_size)!=h.checksum)return -5;
  result=FHD_Restore(g_buffer+sizeof(h),h.data_size);
  return result<0?result:1;
}

/* FHD NPC AI v0.9: a sidecar is bound to exact SAV contents. Older v4 sidecars
   are not imported by this function, preventing stale state after slot moves. */
#pragma pack(push,1)
typedef struct TaggedHeader {
  uint8_t magic[8];
  uint32_t version, data_size, checksum;
  uint64_t galaxy_key, sav_fingerprint;
} TaggedHeader;
#pragma pack(pop)
int32_t FHD_CALL FHD_BridgeSaveTaggedSlot(const uint16_t *path,uint64_t key,
    const void *snapshot,uint32_t bytes,uint64_t fingerprint){
  TaggedHeader h; static const uint8_t magic[8]={'F','H','D','S','A','V','0','9'};
  uint32_t i;
  if(!snapshot||!bytes||!key||!fingerprint||bytes+sizeof(h)>SLOT_CAPACITY||!slot_path(path))return -1;
  for(i=0;i<8;i++)h.magic[i]=magic[i];
  h.version=9u;h.data_size=bytes;h.checksum=checksum((const uint8_t*)snapshot,bytes);
  h.galaxy_key=key;h.sav_fingerprint=fingerprint;
  cp(g_buffer,&h,sizeof(h));cp(g_buffer+sizeof(h),snapshot,bytes);
  if(!WRITEFILE(g_temp_path,g_buffer,bytes+(uint32_t)sizeof(h))){DELETEFILE(g_temp_path);return -2;}
  if(!MOVEFILE(g_temp_path,g_path)){DELETEFILE(g_temp_path);return -3;}
  return 1;
}
int32_t FHD_CALL FHD_BridgeLoadTaggedSlot(const uint16_t *path,uint64_t key,uint64_t fingerprint){
  TaggedHeader h;uint32_t n=0,i;
  static const uint8_t magic[8]={'F','H','D','S','A','V','0','9'};
  /* Fail-closed: loading an unrelated or corrupt slot must clear relations. */
  FHD_BridgeAbortSlot();FHD_Reset();
  if(!key||!fingerprint||!slot_path(path))return -1;
  if(!READFILE(g_path,g_buffer,SLOT_CAPACITY,&n))return -2;
  if(n<sizeof(h))return -3;
  cp(&h,g_buffer,sizeof(h));
  for(i=0;i<8;i++)if(h.magic[i]!=magic[i])return -4;
  if(h.version!=9u||h.data_size!=n-sizeof(h)||h.galaxy_key!=key||
     h.sav_fingerprint!=fingerprint)return -4;
  if(checksum(g_buffer+sizeof(h),h.data_size)!=h.checksum)return -5;
  return FHD_Restore(g_buffer+sizeof(h),h.data_size)>=0?1:-6;
}
/* QuickSave rotation after game's own successful MoveFileW: move a tagged
   sidecar only if its saved SAV fingerprint matches the new destination SAV.
   A failed game-side move never causes us to overwrite or delete memory. */
int32_t FHD_CALL FHD_BridgeMoveTaggedSlotIfMatch(const uint16_t *from,const uint16_t *to,
                                                 uint64_t destination_sav_tag){
 TaggedHeader h;uint32_t n=0;
 if(!destination_sav_tag||!slot_path(from))return 0;
 copy_path(g_move_src,g_path);
 if(!READFILE(g_move_src,g_buffer,SLOT_CAPACITY,&n)||n<sizeof(h))return 0;
 cp(&h,g_buffer,sizeof(h));
 if(h.magic[0]!='F'||h.magic[1]!='H'||h.magic[2]!='D'||h.magic[3]!='S'||
    h.magic[4]!='A'||h.magic[5]!='V'||h.magic[6]!='0'||h.magic[7]!='9'||
    h.version!=9u||h.data_size!=n-sizeof(h)||
    h.sav_fingerprint!=destination_sav_tag||
    checksum(g_buffer+sizeof(h),h.data_size)!=h.checksum)return 0;
 if(!slot_path(to))return 0;
 copy_path(g_move_dst,g_path);
 if(eq_path(g_move_src,g_move_dst))return 1;
 return MOVEFILE(g_move_src,g_move_dst)?1:0;
}
