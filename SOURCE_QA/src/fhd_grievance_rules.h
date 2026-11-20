#ifndef FHD_GRIEVANCE_RULES_H
#define FHD_GRIEVANCE_RULES_H
#include <stdint.h>
#include "fhd_npc_ai.h"
/* Pure and separately testable decisions: 0 ignore, 1 protest,
   2 demand compensation, 3 confront. No game objects touched. */
int32_t FHDNPC_LootNextGradeMemory(uint32_t native_value,int32_t existing_memory);
int32_t FHDNPC_LootNextGradeMemoryMin(uint32_t native_value,int32_t existing_memory,uint32_t minimum);
int32_t FHDNPC_LootReaction(const FHDProfile *p, uint32_t item_destroyed,
                           uint32_t target_is_player,uint32_t stronger_threat);
/* 0 none, 1 conditional revenge request, 2 paid contract, 3 bounty offer. */
int32_t FHDNPC_GrudgePlan(const FHDProfile *p,int32_t relation,
                          uint32_t budget,uint32_t target_strength);
/* 1 only when a verified NPC struck the player before the player's return fire. */
int32_t FHDNPC_JustifiedDefense(uint32_t first_side,uint32_t same_star,
                                uint32_t incident_age,uint32_t active_truce);
#endif
