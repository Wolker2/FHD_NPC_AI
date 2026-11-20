#include "fhd_grievance_rules.h"
int32_t FHDNPC_LootNextGradeMemoryMin(uint32_t native_value,int32_t existing_memory,uint32_t minimum){
 int32_t current,target;
 if(native_value<10u||native_value>100u)return 1000; /* not a normal native relation */
 current=(int32_t)native_value+existing_memory;
 if(current>100)current=100;
 if(current<0)current=0;
 /* A new violation may hold a civilian at bad, never improve a relation
    made hostile by a separate, more serious incident. */
 if(minimum>=10u && current<=10)return existing_memory;
 /* Verified repeated interference may ultimately cross the hostile threshold. */
 if(current>=80)target=79;
 else if(current>=60)target=59;
 else if(current>=30)target=29;
 else if(current>=10)target=minimum>=10u?10:5;
 else target=minimum>=10u?10:0;
 if(target<(int32_t)minimum)target=(int32_t)minimum;
 return target-(int32_t)native_value;
}
int32_t FHDNPC_LootNextGradeMemory(uint32_t native_value,int32_t existing_memory){
 return FHDNPC_LootNextGradeMemoryMin(native_value,existing_memory,0u);
}
int32_t FHDNPC_LootReaction(const FHDProfile *p,uint32_t destroyed,
                           uint32_t player,uint32_t stronger){
 int32_t heat;
 if(!p)return 0;
 heat=(int32_t)p->traits[2]+(int32_t)p->traits[6]
     +(int32_t)p->traits[1]+(int32_t)p->traits[5]/2;
 if(destroyed)heat+=23;
 /* Each verified profession has its own default reaction. A liner
    avoids confrontation, a diplomat protests, traders seek restitution,
    while armed mercenaries, pirates and military become confrontational.
    Unverified roles fall back to the character's traits. */
 switch(p->role){
   case FHD_ROLE_TRADER: heat-=15;break;
   case FHD_ROLE_LINER: heat-=75;break;
   case FHD_ROLE_DIPLOMAT: heat-=42;break;
   case FHD_ROLE_MILITARY: heat+=35;break;
   case FHD_ROLE_PIRATE: heat+=52;break;
   case FHD_ROLE_SMUGGLER: heat+=35;break;
   case FHD_ROLE_MERCENARY: heat+=32;break;
   case FHD_ROLE_AGENT: heat-=22;break;
   case FHD_ROLE_SPECIAL_AGENT: heat+=15;break;
   case FHD_ROLE_SPY: heat-=40;break;
   default: break;
 }
 if(p->character==FHD_BANDIT || p->character==FHD_CORSAIR ||
    p->character==FHD_RAIDER)heat+=18;
 if(stronger)heat-=65;
 if(heat<92)return 0;
 if(heat<168)return 1;
 if((p->role==FHD_ROLE_PIRATE || p->role==FHD_ROLE_SMUGGLER ||
     p->character==FHD_BANDIT || p->character==FHD_CORSAIR) && player){
    if(heat>288 && p->traits[0]>68 && !stronger)return 3;
    return 2;
 }
 return 1;
}
int32_t FHDNPC_GrudgePlan(const FHDProfile*p,int32_t relation,
                         uint32_t budget,uint32_t threat){
 uint32_t will;
 if(!p||relation>-28||budget<500||!threat)return 0;
 will=(uint32_t)p->traits[6]*2u+(uint32_t)p->traits[5]+p->traits[1];
 if(will<155u)return 0;
 if(budget<threat/20u+600u)return 1;
 if(will>=266u && budget>threat/3u+2000u)return 3;
 if(will>=202u)return 2;
 return 1;
}
int32_t FHDNPC_JustifiedDefense(uint32_t first,uint32_t same_star,
                               uint32_t age,uint32_t truce){
 return first==1u && same_star && age<=15u && !truce;
}
