#include "fhd_world.h"

enum {SYSTEMS=256, MARKETS=512};
typedef struct {uint32_t star,day,risk,delivery_day;} System;
typedef struct {uint32_t planet,star,day;uint16_t price[8],need[8];} Market;
typedef struct {uint32_t from,to,good,remaining,expires,created;} Agreement;
static struct {System systems[SYSTEMS]; Market markets[MARKETS]; Agreement agreements[64];} world;
_Static_assert(sizeof(world)==FHD_WORLD_BYTES,"persistent world layout");

static void bytes(void *destination,const void *source,unsigned size){
  uint8_t *out=destination;
  const uint8_t *in=source;
  for(unsigned i=0;i<size;i++)out[i]=in?in[i]:0;
}
void FHD_WorldReset(void){bytes(&world,0,sizeof(world));}
static System *system(uint32_t star){
  for(unsigned i=0;i<SYSTEMS;i++){
    System *s=&world.systems[(star+i)&(SYSTEMS-1)];
    if(s->star==star||!s->star)return s;
  }
  return &world.systems[star&(SYSTEMS-1)];
}
static Market *market(uint32_t planet){
  for(unsigned i=0;i<MARKETS;i++){
    Market *m=&world.markets[(planet+i)&(MARKETS-1)];
    if(m->planet==planet||!m->planet)return m;
  }
  return &world.markets[planet&(MARKETS-1)];
}
unsigned FHD_WorldRisk(uint32_t star,uint32_t day){
  const System *s=system(star);
  if(!star||s->star!=star||day<s->day)return 0;
  uint32_t age=(day-s->day)/2;
  return age>=s->risk?0:s->risk-age;
}
void FHD_WorldAttack(uint32_t star,uint32_t day){
  if(!star||!day)return;
  System *s=system(star);
  if(s->star==star&&s->day>day)return;
  unsigned score=FHD_WorldRisk(star,day)+18;
  if(s->star!=star)*s=(System){star,0,0,0};
  s->day=day;s->risk=score>100?100:score;
}
void FHD_WorldDelivery(uint32_t star,uint32_t day,unsigned quantity){
  /* A shipment is evidence of resumed traffic; it does not erase the attacks. */
  System *s=system(star);
  unsigned score=FHD_WorldRisk(star,day);
  if(!quantity||s->star!=star||!score)return;
  unsigned reduction=quantity>=100?3:1;
  if(s->delivery_day!=day){
    s->day=day;s->risk=score>reduction?score-reduction:0;s->delivery_day=day;
  }
}
void FHD_WorldObserve(uint32_t planet,uint32_t star,uint32_t day,
                      const uint16_t *prices,const uint16_t *needs){
  if(!planet||!star||!day)return;
  Market *m=market(planet);
  if(m->planet==planet&&m->star==star&&m->day>day)return;
  m->planet=planet;m->star=star;m->day=day;
  for(unsigned i=0;i<8;i++){m->price[i]=prices[i];m->need[i]=needs[i];}
}
int FHD_WorldQuote(uint32_t planet,uint32_t star,uint32_t day,unsigned good,
                   unsigned *price,unsigned *need){
  const Market *m=market(planet);
  if(good>=8||!planet||!star||m->planet!=planet||m->star!=star||day<m->day||day-m->day>50||
     !m->price[good])return 0;
  unsigned age=day-m->day;
  *price=m->price[good]*(100-age)/100;
  *need=m->need[good]*(50-age)/50;
  return 1;
}
void FHD_WorldStats(uint32_t day,uint32_t *out){
  out[0]=0;out[1]=0;out[2]=0;out[3]=0;
  for(unsigned i=0;i<SYSTEMS;i++){
    const System *s=&world.systems[i];
    if(s->star&&day>=s->day&&(day-s->day)/2<s->risk)out[0]++;
    if(s->star&&s->delivery_day==day)out[3]++;
  }
  for(unsigned i=0;i<MARKETS;i++){
    const Market *m=&world.markets[i];
    if(!m->planet||day<m->day||day-m->day>50)continue;
    out[1]++;
    for(unsigned g=0;g<8;g++)if(m->need[g]*(50-(day-m->day))>=2000){
      out[2]++;
      break;
    }
  }
}
const void *FHD_WorldBytes(void){return &world;}
static Agreement *find_agreement(uint32_t from,uint32_t to,unsigned good,uint32_t day){
  for(unsigned i=0;i<64;i++){
    Agreement *a=&world.agreements[i];
    if(a->from==from&&a->to==to&&a->good==good&&a->created<=day&&a->expires>=day)return a;
  }
  return 0;
}
int FHD_WorldAgree(uint32_t from,uint32_t to,unsigned good,unsigned quantity,uint32_t day){
  if(!from||!to||from==to||good>=8||!quantity||quantity>400||!day||
     find_agreement(from,to,good,day))return 0;
  for(unsigned i=0;i<64;i++){
    Agreement *a=&world.agreements[i];
    if(!a->from||a->expires<day){
      *a=(Agreement){from,to,good,quantity,day+35,day};return 1;
    }
  }
  return 0;
}
unsigned FHD_WorldAgreement(uint32_t from,uint32_t to,unsigned good,uint32_t day){
  const Agreement *a=find_agreement(from,to,good,day);
  return a?a->remaining:0;
}
unsigned FHD_WorldFulfil(uint32_t from,uint32_t to,unsigned good,unsigned quantity,uint32_t day){
  Agreement *a=find_agreement(from,to,good,day);
  if(!a||!a->remaining||!quantity)return 0;
  a->remaining-=quantity<a->remaining?quantity:a->remaining;
  return a->remaining?1u:2u;
}
int FHD_WorldValidate(const void *source,unsigned size){
  const uint8_t *p=source;
  for(unsigned i=0;i<SYSTEMS;i++){
    System s;
    bytes(&s,p+i*sizeof(s),sizeof(s));
    if(s.risk>100||(!s.star&&(s.risk||s.day||s.delivery_day)))return 0;
  }
  if(size==FHD_WORLD_BYTES)for(unsigned i=0;i<64;i++){
    Agreement a;
    bytes(&a,p+FHD_WORLD_V6_BYTES+i*sizeof(a),sizeof(a));
    if(!a.from){if(a.to||a.remaining||a.expires||a.created)return 0;continue;}
    if(!a.to||a.from==a.to||a.good>=8||a.remaining>400||!a.created||
       a.expires<a.created||a.expires-a.created!=35)return 0;
  }
  return 1;
}
void FHD_WorldLoad(const void *source,unsigned size){bytes(&world,source,size);}
