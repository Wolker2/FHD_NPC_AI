/* Pure deterministic, integer policy. Risk 0..100 is derived by the verified
   host from cached strengths; never treats a weak enemy as inherently safe.
   No game state is mutated by this module. */
#include "fhd_social_rules.h"
int32_t FHDNPC_AssessEnemy(const FHDProfile *p,int32_t relation,
 uint32_t kind,uint32_t dominator,uint32_t risk,int32_t *score){
 int32_t value;
 if(score)*score=0;
 if(!p||kind>1u||risk>100u)return FHD_TACTICAL_ALLOW;
 if(!dominator && relation>=45){if(score)*score=999;return FHD_TACTICAL_AVOID_FRIEND;}
 /* The score models danger, caution, combat appetite and opportunity cost.
    Positive relationships discourage attacking; repeated attacks make
    retaliation more likely. A soldier needs a stronger reason to disengage. */
 value=2*(int32_t)p->traits[3]+(int32_t)risk
     -2*(int32_t)p->traits[0]-(int32_t)p->traits[1];
 if(kind==0u)value+=(int32_t)p->traits[2]/5; /* trading opportunity cost */
 if(relation>0)value+=relation/2;
 else if(relation<0)value+=relation/3;
 if(score)*score=value;
 if(value>=(kind==0u?118:158))return FHD_TACTICAL_AVOID_RISK;
 return FHD_TACTICAL_ALLOW;
}
int32_t FHDNPC_ShouldAvoidEnemy(const FHDProfile*p,int32_t rel,
 uint32_t kind,uint32_t dom){
 return FHDNPC_AssessEnemy(p,rel,kind,dom,50u,0)!=FHD_TACTICAL_ALLOW;
}

int32_t FHDNPC_PartnershipCredit(uint32_t verified_contract,int32_t current_relation){
 return verified_contract && current_relation==0;
}
int32_t FHDNPC_JointCombatCredit(uint32_t same_victim_day,uint32_t active_contract,
                                uint32_t both_managed){
 return same_victim_day && active_contract && both_managed;
}
int32_t FHDNPC_EscortCredit(uint32_t contract,uint32_t follows_partner,
 uint32_t verified_partner,uint32_t day,uint32_t last_event_day,int32_t relation){
 return contract && follows_partner && verified_partner && day &&
        day!=last_event_day && relation>0 && relation<45;
}
