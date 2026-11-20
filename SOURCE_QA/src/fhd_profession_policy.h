#ifndef FHD_PROFESSION_POLICY_H
#define FHD_PROFESSION_POLICY_H
#include <stdint.h>
#include "fhd_npc_ai.h"
/* Native ship class (TShip.TypeId) + verified per-class byte at 0x510.
   Unknown profession names are deliberately NOT inferred from careers. */
enum FHDProfession {
 FHD_PROF_UNKNOWN=0, FHD_PROF_RANGER=1, FHD_PROF_TRADER=2,
 FHD_PROF_LINER=3, FHD_PROF_DIPLOMAT=4, FHD_PROF_FREE_PIRATE=5,
 FHD_PROF_CLAN_PIRATE=6, FHD_PROF_WARRIOR=7, FHD_PROF_FLAGSHIP=8,
 FHD_PROF_STATION=9,
 FHD_PROF_VERIFIED_AGENT=10, FHD_PROF_VERIFIED_SPECIAL_AGENT=11,
 FHD_PROF_VERIFIED_SMUGGLER=12, FHD_PROF_VERIFIED_MERCENARY=13,
 FHD_PROF_VERIFIED_SPY=14
};
enum FHDPirateMotive {
 FHD_PIRATE_NO_VERIFIED_MOTIVE=0, FHD_PIRATE_REVENGE=1,
 FHD_PIRATE_RANSOM_OR_CARGO=2, FHD_PIRATE_CLAN_DUTY=3,
 FHD_PIRATE_PROTECT_FRIEND=4
};
uint32_t FHDNPC_Profession(uint32_t native_type,uint32_t native_subtype);
uint32_t FHDNPC_RoleFromProfession(uint32_t profession);
/* Read-only economic threshold: money is a real native field, not evidence
   of cargo. This only vetoes *new* optional robberies after vanilla selected
   an enemy; it does not overwrite existing retaliation or clan warfare. */
uint32_t FHDNPC_PirateMotive(const FHDProfile *p,int32_t relation,
  uint32_t pirate_subtype,int32_t target_money,uint32_t cargo_present,
  uint32_t is_retal, uint32_t target_player);
/* A verified repeated planet-hop can be interrupted only if the pirate is
   strong enough to stand a chance. Both strengths must be valid and the ship
   must already have a live enemy; otherwise never turn off fear. */
/* Cunning robbers prefer easy prey to unprofitable losing battles.
   Does not fabricate a bluff, ransom message or successful negotiation. */
uint32_t FHDNPC_AvoidStrongPrey(const FHDProfile*p,uint32_t motive,
  uint32_t own_strength,uint32_t enemy_strength);
uint32_t FHDNPC_InterruptHop(uint32_t hops,uint32_t has_enemy,
  uint32_t armed,uint32_t own_strength,uint32_t enemy_strength);
/* Confirmed native attacker may change a transport's existing fear outcome.
   This deliberately reuses vanilla escape/ransom/repair. 0 means no override. */
uint32_t FHDNPC_MemoryFear(uint32_t profession, const FHDProfile *p,
  int32_t relation, uint32_t own_strength, uint32_t enemy_strength,
  uint32_t hull_integrity_percent);
uint32_t FHDNPC_AttackMemoryWeight(uint32_t profession);
/* Profession-aware scoring for externally verified agent/spy/smuggler/
   mercenary roles. Availability and execution are still provided by host. */
int32_t FHDNPC_RoleDecisionBonus(const FHDProfile *p,uint32_t action);
uint32_t FHDNPC_TrustKnownAggressor(uint32_t original_trust,
  int32_t relation, uint32_t profession);
/* Explicit native profile tagging API for external NPC mods. This does not
   pretend to detect a spy/agent from a generic TShip type. Tags are in-memory
   and must be supplied by the owning mod for each active session. */
#if defined(_WIN32)
#define FHD_PROF_API __declspec(dllexport)
#else
#define FHD_PROF_API
#endif
FHD_PROF_API int32_t FHDNPC_RegisterVerifiedProfession(uint32_t id,uint32_t seed,
  uint32_t native_type,uint32_t profession);
FHD_PROF_API uint32_t FHDNPC_LookupVerifiedProfession(uint32_t id,uint32_t seed,
  uint32_t native_type);
FHD_PROF_API void FHDNPC_ResetVerifiedProfessions(void);

#endif
