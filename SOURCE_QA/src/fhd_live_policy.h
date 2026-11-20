#ifndef FHD_LIVE_POLICY_H
#define FHD_LIVE_POLICY_H
#include <stdint.h>
#include "fhd_npc_bridge.h"
#pragma pack(push,1)
typedef struct FHDLiveFrame {
 uint32_t id,seed,day;
 int32_t money,kills;
 uint32_t action,valid;
 uint8_t old_career,old_aggression,used_career,used_aggression;
 uint8_t changed,pirate_changed,reserved[2];
 uint32_t old_pressure_bits,used_pressure_bits;
} FHDLiveFrame;
#pragma pack(pop)
typedef struct FHDLiveStats {
 uint32_t considered, registered, excluded, ranger_choices, ranger_applied;
 uint32_t inferred_profit, inferred_loss, confirmed_kills;
 uint32_t action_counts[10];
 uint32_t categories[6];
 uint32_t skip_invalid,skip_provenance,skip_scripts,skip_special,skip_identity;
 uint32_t skip_profile,skip_no_action,skip_unsupported,skip_busy,skip_invalid_preferences;
 uint32_t skip_no_effect,skip_invalid_pressure,changed_pirates,game_writes_preserved;
 uint32_t category_choices[6],category_applied[6];
 uint32_t relationship_target_eligible,relationship_target_nonzero;
 uint32_t relationship_action_changed,relationship_action_applied;
} FHDLiveStats;
void FHDNPC_LiveBefore(uint8_t *ship,uint32_t category,FHDLiveFrame *frame);
void FHDNPC_LiveAfter(uint8_t *ship,uint32_t category,FHDLiveFrame *frame);
void FHDNPC_LiveReset(void);
void FHDNPC_LiveStatsGet(FHDLiveStats *out);
#endif
