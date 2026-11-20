/* Only verified in-system pirate attacks create station news.
 * Source: TShip.ReactToAttack, after its same-day pair deduplication.
 * No synthetic off-screen ship creation or unverified arrest orders. */
#include <stdint.h>
#if defined(_WIN32) && defined(_M_IX86)
#define DLLIMPORT __declspec(dllimport)
#define STDCALL __stdcall
#else
#error Windows x86 only
#endif
DLLIMPORT void *STDCALL GetModuleHandleW(const uint16_t*);
extern uint32_t FHDNPC_StationBroadcast(const void*,const void*,uint32_t,uint32_t);
extern void FHDNPC_InvokeJoint(void*,void*,void*);
#define LIMIT 128u
#define MAX_SHIPS 4096u
#define STATION_VMT 0x29a3a8u /* station native VMT start */
#define MILITARY_VMT 0x1a2b10u
struct Incident {uint32_t sid,ss,offender,os,star,last,seen,reported,dispatched,need;};
static struct Incident incidents[LIMIT];
static uint32_t counters[4]; /* verified pair, report, dispatch, dropped */
static uint32_t u32(const void*p,unsigned o){const uint8_t*b=(const uint8_t*)p+o;
 return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void *ptr(const void*p,unsigned o){return (void*)(uintptr_t)u32(p,o);}
static int active(const uint8_t*p){return p && u32(p,4u)&&u32(p,4u)!=0xffffffffu &&
 !p[0x459u]&&!ptr(p,0x1cu)&&!ptr(p,0x20u);}
static int station_ok(const uint8_t*p,const uint8_t*exe){
 return p && (p[0x10u]==6u||p[0x10u]==8u)&&
     ptr(p,0)==exe+STATION_VMT && !ptr(p,0x3ccu)&&!p[0x44cu]&&!p[0x43cu];
}
static int pirate_ok(const uint8_t*p,const uint8_t*exe){
 return active(p)&&p[0x10u]==3u&&ptr(p,0)==exe+0x1992b0u&&
 !ptr(p,0x3ccu)&&!p[0x44cu]&&!p[0x43cu];
}
static uint8_t *find_ship(const uint8_t *star,uint32_t id,uint32_t seed){
 const uint8_t*list=ptr(star,0x2cu);if(!list)return 0;
 uint32_t n=u32(list,8u);if(n>MAX_SHIPS)return 0;
 const uint32_t *arr=ptr(list,4u);if(!arr)return 0;
 for(uint32_t j=0;j<n;j++){
   uint8_t*p=(uint8_t*)(uintptr_t)arr[j];
   if(p&&u32(p,4u)==id&&u32(p,0xe0u)==seed&&ptr(p,0x24u)==star)return p;
 }
 return 0;
}
static struct Incident *slot_for(uint32_t sid,uint32_t off,uint32_t day){
 uint32_t h=(sid*2654435761u ^ off*2246822519u)&(LIMIT-1u);
 for(uint32_t i=0;i<LIMIT;i++){
   struct Incident *s=&incidents[(h+i)&(LIMIT-1u)];
   if(s->sid==sid&&s->offender==off)return s;
   if(!s->sid||(day>=s->last&&day-s->last>60u))return s;
 }
 counters[3]++;return 0;
}
void FHDNPC_StationObservePirateAttack(const void*victim,const void*attacker,uint32_t day){
 const uint8_t *v=victim,*p=attacker,*exe=GetModuleHandleW(0),*star;
 if(!v||!p||!exe||!day||day>1000000u||!pirate_ok(p,exe)||
    !(star=ptr(v,0x24u))||ptr(p,0x24u)!=star)return;
 const uint8_t *list=ptr(star,0x2cu);if(!list)return;
 uint32_t n=u32(list,8u);if(!n||n>MAX_SHIPS)return;
 const uint32_t *arr=ptr(list,4u);if(!arr)return;
 for(uint32_t i=0;i<n;i++){
   uint8_t*s=(uint8_t*)(uintptr_t)arr[i];
   if(!station_ok(s,exe))continue;
   uint32_t sid=u32(s,4u),star_id=u32(star,4u),pid=u32(p,4u);
   /* Aggregate recent pirate attacks observed by the same station in this star
      system so a wave of 3-4 pirate raids triggers a station bulletin even when
      multiple raiders share the attacks. */
   uint32_t recent_system_attacks=0u;
   for(uint32_t k=0;k<LIMIT;k++){
     struct Incident *other=&incidents[k];
     if(other->sid==sid&&other->star==star_id&&other->offender!=pid&&
        day>=other->last&&day-other->last<=20u&&other->seen>0u){
       if(other->seen<other->need)other->seen++;
       if(other->seen>recent_system_attacks)recent_system_attacks=other->seen;
     }
   }
   struct Incident *e=slot_for(sid,pid,day);if(!e)continue;
   if(e->sid!=sid||e->ss!=u32(s,0xe0u)||
      e->offender!=pid||e->os!=u32(p,0xe0u)||
      e->star!=star_id){
     *e=(struct Incident){sid,u32(s,0xe0u),pid,
         u32(p,0xe0u),star_id,day,recent_system_attacks,0u,0u,
         3u+((u32(p,4u)^u32(s,4u))&1u)};
   }
   if(day<e->last||day-e->last>30u){e->seen=0u;e->reported=0u;e->dispatched=0u;}
   e->last=day;
   if(e->seen<e->need)e->seen++;
   counters[0]++;
 }
}
static uint32_t dispatch_one(const uint8_t*station,uint8_t*attacker,const uint8_t*exe){
 const uint8_t*star=ptr(station,0x24u),*list=star?ptr(star,0x2cu):0;
 if(!list)return 0;
 uint32_t n=u32(list,8u);if(!n||n>MAX_SHIPS)return 0;
 const uint32_t*arr=ptr(list,4u);if(!arr)return 0;
 for(uint32_t i=0;i<n;i++){
   uint8_t*w=(uint8_t*)(uintptr_t)arr[i];
   if(!active(w)||w[0x10u]!=4u||ptr(w,0)!=exe+MILITARY_VMT||
      w[0x11u]>4u||ptr(w,0x3ccu)||w[0x44cu]||w[0x43cu]||
      ptr(w,0x3f4u)||ptr(w,0x3f8u)||ptr(w,0x3fcu)||
      w[0x3b4u]||ptr(w,0x24u)!=star)continue;
   FHDNPC_InvokeJoint(w,w,attacker);
   if(ptr(w,0x3f4u)==attacker){counters[2]++;return 1;}
   return 0;
 }
 return 0;
}
void FHDNPC_StationTick(void *station){
 const uint8_t *s=station,*exe=GetModuleHandleW(0),*star;
 if(!exe||!station_ok(s,exe)||!(star=ptr(s,0x24u)))return;
 for(uint32_t i=0;i<LIMIT;i++){
   struct Incident *e=&incidents[i];
   if(e->sid!=u32(s,4u)||e->ss!=u32(s,0xe0u)||
      e->star!=u32(star,4u)||e->seen<e->need||e->reported)continue;
   uint32_t day=u32(s,0xecu);
   if(day<e->last||day-e->last>20u){e->reported=1u;continue;}
   uint8_t *offender=find_ship(star,e->offender,e->os);
   if(!pirate_ok(offender,exe))continue;
   if((s[0x10u]==8u||s[0x10u]==6u) && !e->dispatched)
      e->dispatched=dispatch_one(s,offender,exe);
   if(FHDNPC_StationBroadcast(s,offender,day,e->dispatched)){
     e->reported=1u;counters[1]++;
   }
 }
}
void FHDNPC_StationStats(uint32_t*out){if(out)for(uint32_t i=0;i<4u;i++)out[i]=counters[i];}
void FHDNPC_StationDiagnostics(uint32_t*out){
 if(!out)return;
 for(uint32_t j=0;j<4u;j++)out[j]=0u;
 for(uint32_t j=0;j<LIMIT;j++){
   const struct Incident*e=&incidents[j];
   if(!e->sid||!e->offender)continue;
   out[0]++;
   if(e->reported)out[3]++;
   else if(e->seen>=e->need)out[2]++;
   else out[1]++;
 }
}

/* Global, read-only population audit. The engine simulates every TStar
   daily; count real ships before any plan to modify native spawning. */
void FHDNPC_PopulationStats(uint32_t*out){
 if(!out)return;
 for(uint32_t j=0;j<7u;j++)out[j]=0;
 const uint8_t*exe=GetModuleHandleW(0);
 const uint8_t*galaxy=exe?ptr(exe,0x41d2a0u):0;
 const uint8_t*list=galaxy?ptr(galaxy,0x2cu):0;
 if(!list)return;
 uint32_t count=u32(list,8u);
 if(!count||count>4096u)return;
 const uint32_t *arr=ptr(list,4u);if(!arr)return;
 for(uint32_t i=0;i<count;i++){
   const uint8_t*star=(const uint8_t*)(uintptr_t)arr[i];if(!star)continue;
   out[0]++;
   const uint8_t*ships=ptr(star,0x2cu);
   if(!ships){out[2]++;continue;}
   uint32_t n=u32(ships,8u);if(n>MAX_SHIPS)continue;
   const uint32_t*all=ptr(ships,4u);if(!all)continue;
   uint32_t open=0u;
   for(uint32_t j=0;j<n;j++){
     const uint8_t*p=(const uint8_t*)(uintptr_t)all[j];
     if(!p||p[0x10u]<1u||p[0x10u]>4u)continue;
     out[6]++;
     if(p[0x459u])out[5]++;
     else if(ptr(p,0x1cu)||ptr(p,0x20u))out[4]++;
     else {open++;out[3]++;}
   }
   if(open)out[1]++;else out[2]++;
 }
}
void FHDNPC_StationReset(void){
 for(uint32_t i=0;i<LIMIT;i++)incidents[i].sid=0u;
 for(uint32_t i=0;i<4u;i++)counters[i]=0u;
}
