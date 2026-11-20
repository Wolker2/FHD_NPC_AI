#ifndef FHD_SOCIAL_RULES_H
#define FHD_SOCIAL_RULES_H
#include <stdint.h>
#include "fhd_npc_ai.h"
/* Personality can veto an existing or new enemy target even during a native
   mission. The host must still enforce NPC identity and provenance. */
enum FHD_TacticalOutcome {
 FHD_TACTICAL_ALLOW=0, FHD_TACTICAL_AVOID_FRIEND=1,
 FHD_TACTICAL_AVOID_RISK=2
};
int32_t FHDNPC_AssessEnemy(const FHDProfile *profile,int32_t relation,
 uint32_t transport_or_warrior,uint32_t enemy_is_dominator,uint32_t risk,
 int32_t *score);
/* Verified signals only: a current contract with no prior relationship; or
   two registered partners confirmed firing on the same target that day. */
int32_t FHDNPC_PartnershipCredit(uint32_t verified_contract,int32_t current_relation);
int32_t FHDNPC_JointCombatCredit(uint32_t same_victim_day,uint32_t active_contract,
                                uint32_t both_managed);
/* Confirmed native follow-ship order to an active contractual partner.
   The day guard lives in the existing persisted contact (no separate cache). */
int32_t FHDNPC_EscortCredit(uint32_t contract,uint32_t follows_partner,
 uint32_t verified_partner,uint32_t day,uint32_t last_event_day,int32_t relation);
/* Legacy pure-policy entry kept for 1.0 test ABI compatibility. */
int32_t FHDNPC_ShouldAvoidEnemy(const FHDProfile *profile,int32_t relation,
 uint32_t transport_or_warrior,uint32_t enemy_is_dominator);
#endif
