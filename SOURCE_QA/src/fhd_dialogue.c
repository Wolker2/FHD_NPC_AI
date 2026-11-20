#include "fhd_dialogue.h"
#include "fhd_dialogue_corpus.h"
#define QUEUE_CAP 6u
#define NAME_CAP 64u
#define DEDUP_CAP QUEUE_CAP
struct DialogueItem {uint32_t sender,target,personal;uint16_t line;uint16_t name[NAME_CAP];};
static struct DialogueItem q[QUEUE_CAP];
static uint8_t used[FHD_DIALOGUE_HISTORY_BYTES];
static uint32_t rd,wr,mode=FHD_DLG_PLAYER;
static uint32_t keys[DEDUP_CAP],key_count;
static uint32_t current_system,last_day,per_day;
extern void FHDNPC_DialogueSettingsSave(uint32_t mode);
static uint32_t counters[6]; /* received,queued,shown,deduplicated,filtered,suppressed */
_Static_assert(FHD_AUTHORED_LINES<=FHD_DIALOGUE_HISTORY_BYTES*8u,"dialogue history capacity");
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
int32_t FHDNPC_DialogueModeSet(uint32_t value){
 if(value>FHD_DLG_LOCAL)return 0;
 if(mode!=value){
   /* A settings change takes effect immediately, not after the old daily quota. */
   per_day=0u;key_count=0u;rd=wr=0u;
 }
 mode=value;
 FHDNPC_DialogueSettingsSave(value);return 1;
}
uint32_t FHDNPC_DialogueModeGet(void){return mode;}
uint32_t FHDNPC_DialogueVariants(void){return FHD_AUTHORED_LINES;}
uint32_t FHDNPC_DialogueQueued(void){return wr-rd;}
const void *FHD_DialogueHistory(void){return used;}
void FHD_DialogueRestoreHistory(const uint8_t *bytes){
 for(unsigned i=0;i<sizeof(used);i++)used[i]=bytes?bytes[i]:0;
}
void FHDNPC_DialogueReset(void){
 rd=wr=current_system=last_day=per_day=key_count=0u;
 FHD_DialogueRestoreHistory(0);
 for(unsigned i=0;i<6u;i++)counters[i]=0u;
}
void FHD_DialogueContext(uint32_t player_system,uint32_t day){
 if(last_day!=day){
   last_day=day;per_day=key_count=0u;rd=wr=0u;
 }
 if(current_system!=player_system){
   current_system=player_system;rd=wr=0u;
 }
}
static unsigned loot_line(unsigned event){
 return event==FHD_DLG_LOOT_TAKEN||event==FHD_DLG_LOOT_DESTROYED||
        event==FHD_DLG_THREAT||event==FHD_DLG_DEMAND||
        event==FHD_DLG_PIRATE_TAKE||event==FHD_DLG_PIRATE_DESTROY;
}
static int line_available(unsigned line){
 if(used[line/8u]&(1u<<(line%8u)))return 0;
 for(uint32_t i=rd;i<wr;i++)if(q[i%QUEUE_CAP].line==line)return 0;
 return 1;
}
int32_t FHD_DialogueNamed(uint32_t event,uint32_t race,uint32_t actor,uint32_t target,
 uint32_t personal,uint32_t day,uint32_t salt,uint32_t system,const uint16_t *name){
 counters[0]++;
 if(!event||event>FHD_DLG_PIRATE_DESTROY||!actor||actor==0xffffffffu)return -1;
 if(!system||system!=current_system||!day||day!=last_day||day>1000000u||
    mode==FHD_DLG_VANILLA||(!personal&&(mode==FHD_DLG_PLAYER||loot_line(event)))){
   counters[4]++;return 0;
 }
 if(race>=5u)race=2u;
 uint32_t key=mix(actor*2654435761u ^ target*2246822519u ^ event*3266489917u ^ salt);
 for(unsigned i=0;i<key_count;i++)if(keys[i]==key){counters[3]++;return 0;}
 if(per_day>=(mode==FHD_DLG_PLAYER?2u:QUEUE_CAP)||wr-rd>=QUEUE_CAP){
   counters[5]++;return 0;
 }
 unsigned first=line_offsets[event-1u][race],count=line_offsets[event-1u][race+1u]-first;
 unsigned line=FHD_AUTHORED_LINES,start=mix(key^day)%count;
 for(unsigned i=0;i<count;i++){
   unsigned candidate=first+(start+i)%count;
   if(line_available(candidate)){line=candidate;break;}
 }
 if(line==FHD_AUTHORED_LINES){
   /* The entire authored group was used in this save. Begin another cycle,
      without repeating an already-queued line; otherwise the radio dies forever. */
   for(unsigned i=first;i<first+count;i++)used[i/8u]&=(uint8_t)~(1u<<(i%8u));
   for(unsigned i=0;i<count;i++){
      unsigned candidate=first+(start+i)%count;
      if(line_available(candidate)){line=candidate;break;}
   }
   if(line==FHD_AUTHORED_LINES){counters[5]++;return 0;}
 }
 keys[key_count++]=key;
 struct DialogueItem *item=&q[wr%QUEUE_CAP];
 item->sender=actor;item->target=target;item->personal=personal;item->line=(uint16_t)line;
 unsigned i=0,in_tag=0;
 if(name){
   for(unsigned k=0;name[k]&&i+1<NAME_CAP;k++){
     uint16_t c=name[k];
     if(c=='<'){in_tag=1;continue;}
     if(c=='>'){in_tag=0;continue;}
     if(in_tag||c<32u)continue;
     /* Strip bounty suffix appended by other mods (e.g. " (награда ...)") */
     if(c=='(' && name[k+1]==0x043du && name[k+2]==0x0430u && name[k+3]==0x0433u)break;
     if(c=='(' && (name[k+1]=='r'||name[k+1]=='R') && name[k+2]=='e' && name[k+3]=='w')break;
     if(c=='(' && name[k+1]==' ' && name[k+2]==0x043du && name[k+3]==0x0430u)break;
     item->name[i++]=c;
   }
 }
 while(i&&item->name[i-1]==' ')i--;
 if(i&&item->name[i-1]>=0xd800u&&item->name[i-1]<=0xdbffu)i--;
 item->name[i]=0;
 wr++;per_day++;counters[1]++;return 1;
}
static uint32_t utf16_append(const char *in,uint16_t*out,uint32_t pos,uint32_t cap){
 const unsigned char*s=(const unsigned char*)in;uint32_t u;
 while(*s){
  if(*s<0x80u){u=*s++;}
  else if((s[0]&0xe0u)==0xc0u && (s[1]&0xc0u)==0x80u){
   u=((uint32_t)(s[0]&31u)<<6)|(s[1]&63u);s+=2;
  }else if((s[0]&0xf0u)==0xe0u && (s[1]&0xc0u)==0x80u && (s[2]&0xc0u)==0x80u){
   u=((uint32_t)(s[0]&15u)<<12)|((uint32_t)(s[1]&63u)<<6)|(s[2]&63u);s+=3;
  }else if((s[0]&0xf8u)==0xf0u && (s[1]&0xc0u)==0x80u && (s[2]&0xc0u)==0x80u && (s[3]&0xc0u)==0x80u){
   u=((uint32_t)(s[0]&7u)<<18)|((uint32_t)(s[1]&63u)<<12)|((uint32_t)(s[2]&63u)<<6)|(s[3]&63u);s+=4;
  }else{s++;continue;}
  if(u>0xffffu){
   u-=0x10000u;
   if(out && pos+2u<cap){out[pos]=(uint16_t)(0xd800u+(u>>10));out[pos+1u]=(uint16_t)(0xdc00u+(u&1023u));}
   pos+=2u;
  }else{
   if(out&&pos+1u<cap)out[pos]=(uint16_t)u;
   pos++;
  }
 }
 return pos;
}
uint32_t FHD_DialogueSender(void){return rd==wr?0:q[rd%QUEUE_CAP].sender;}
uint32_t FHD_DialogueTarget(void){return rd==wr?0:q[rd%QUEUE_CAP].target;}
uint32_t FHD_DialoguePersonal(void){return rd==wr?0:q[rd%QUEUE_CAP].personal;}
int32_t FHDNPC_DialoguePeekW(uint16_t *text,uint32_t capacity){
 const struct DialogueItem *item;uint32_t n=0u;
 if(rd==wr)return 0;
 item=&q[rd%QUEUE_CAP];
 if(item->name[0]){
   n=utf16_append("<color=187,154,38>",text,n,capacity);
   for(unsigned i=0;item->name[i];i++,n++)
     if(text&&n<capacity)text[n]=item->name[i];
   n=utf16_append(":</color> ",text,n,capacity);
 }
 n=utf16_append(lines[item->line],text,n,capacity);
 if(n+1u>capacity||!text)return -(int32_t)(n+1u);
 text[n]=0u;return (int32_t)n;
}
int32_t FHDNPC_DialogueAcknowledge(void){
 if(rd==wr)return 0;
 unsigned line=q[rd%QUEUE_CAP].line;
 used[line/8u]|=(uint8_t)(1u<<(line%8u));
 rd++;counters[2]++;return 1;
}
int32_t FHDNPC_DialoguePopW(uint16_t *text,uint32_t capacity){
 int32_t n=FHDNPC_DialoguePeekW(text,capacity);
 if(n>0)(void)FHDNPC_DialogueAcknowledge();
 return n;
}
uint32_t FHDNPC_DialogueStats(uint32_t *out,uint32_t cap){
 uint32_t i;if(!out||cap<6u)return 0u;
 for(i=0;i<6u;i++)out[i]=counters[i];return 1u;
}
