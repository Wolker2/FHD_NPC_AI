#ifndef FHD_NPC_AI_H
#define FHD_NPC_AI_H
#include <stdint.h>
#if defined(_WIN32)
# define FHD_API __declspec(dllexport)
# define FHD_CALL __cdecl
#else
# define FHD_API __attribute__((visibility("default")))
# define FHD_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* ABI 3, fixed-size PODs for explicit, host-controlled integration. */
#define FHD_ABI_VERSION 3
#define FHD_MAX_AGENTS 8192
#define FHD_MAX_RELATIONS 16
#define FHD_FLAG_PLAYER 1u
#define FHD_FLAG_KLISSAN 2u
#define FHD_FLAG_DOMINATOR 4u
#define FHD_FLAG_SCRIPT_LOCK 8u
#define FHD_FLAG_STORY_LOCK 16u
#define FHD_FLAG_CLASSIFIED 32u /* required for unknown/mod ship types */
/* Original game's ship type 0 is dominator. Other values are supplied by bridge. */
#define FHD_SHIP_DOMINATOR 0
#define FHD_SHIP_TRANCLUCATOR 5
#define FHD_STATION_DOMINION 12

enum FHDCharacter {
  FHD_BANDIT=0, FHD_BRUISER, FHD_CORSAIR, FHD_DEALER, FHD_MAGNATE,
  FHD_GANGSTER, FHD_RAIDER, FHD_PEDDLER, FHD_MERCENARY, FHD_WARRIOR, FHD_HERO
};
enum FHDRole {
  FHD_ROLE_UNKNOWN=0, FHD_ROLE_RANGER=1, FHD_ROLE_TRADER=2,
  FHD_ROLE_PIRATE=3, FHD_ROLE_MILITARY=4, FHD_ROLE_DIPLOMAT=5,
  FHD_ROLE_SMUGGLER=6, FHD_ROLE_MERCENARY=7, FHD_ROLE_TRANSPORT=8,
  FHD_ROLE_STATION=9, FHD_ROLE_SPECIAL=10, FHD_ROLE_LINER=11,
  FHD_ROLE_AGENT=12, FHD_ROLE_SPECIAL_AGENT=13, FHD_ROLE_SPY=14,
  FHD_ROLE_MOD=127
};
enum FHDEvent {
  FHD_EVENT_HELP=1, FHD_EVENT_ATTACK=2, FHD_EVENT_BETRAY=3,
  FHD_EVENT_VICTORY=4, FHD_EVENT_DEFEAT=5, FHD_EVENT_PROFIT=6,
  FHD_EVENT_LOSS=7, FHD_EVENT_TRADE=8, FHD_EVENT_EXPLORE=9,
  FHD_EVENT_RESCUE=10, FHD_EVENT_DIPLOMACY=11,
  FHD_EVENT_EXTORTION=12, FHD_EVENT_LOOT_TAKEN=13, FHD_EVENT_LOOT_DESTROYED=14
};
enum FHDAction {
  FHD_ACTION_NONE=0, FHD_ACTION_TRADE=1, FHD_ACTION_PIRACY=2,
  FHD_ACTION_FIGHT=3, FHD_ACTION_REPAIR=4, FHD_ACTION_FLEE=5,
  FHD_ACTION_DEFEND_ALLY=6, FHD_ACTION_EXPLORE=7,
  FHD_ACTION_DIPLOMACY=8, FHD_ACTION_MISSION=9
};
#pragma pack(push,1)
typedef struct FHDProfile {
  uint32_t id;
  uint8_t character, role;
  /* bravery, aggression, greed, caution, honesty, cunning, vengefulness,
     loyalty, sociability, curiosity; 0..100 */
  uint8_t traits[10];
  uint8_t profession[3]; /* pirate/trade/military sum = 100 */
  uint8_t reserved;
  uint32_t events;
} FHDProfile;
typedef struct FHDSituation {
  uint32_t target_id;  /* 0 = unspecified */
  uint8_t danger, expected_profit, hull_damage, relative_enemy_power;
  uint8_t combat_available, piracy_available, trading_available;
  uint8_t repair_available, flee_available, ally_in_danger;
  uint8_t explore_available, diplomacy_available, mission_available;
  uint8_t reserved[3];
} FHDSituation;
typedef struct FHDDecision {
  uint32_t action;
  int32_t score;
  int32_t relation_to_target;
} FHDDecision;
#pragma pack(pop)

FHD_API uint32_t FHD_CALL FHD_Version(void);
FHD_API void FHD_CALL FHD_Reset(void);
/* 1 managed; 0 protected/exempt (player/dominator/klissan/story); negative error.
   native_type is the original TShipType; owner/faction flags are provided by caller.
   classification must be refreshed when a mod spawns a ship. */
FHD_API int32_t FHD_CALL FHD_Upsert(uint32_t id, uint32_t seed, int32_t character,
   uint32_t role, uint32_t native_type, uint32_t flags,
   uint32_t pirate, uint32_t trade, uint32_t military);
FHD_API int32_t FHD_CALL FHD_ProfileOf(uint32_t id, FHDProfile *out);
int32_t FHD_ProfileOfIdentity(uint32_t id, uint32_t seed, FHDProfile *out);
void FHD_BindLegacyPlayer(uint32_t id, uint32_t seed);
/* Remove retired ship state; other agents may retain memories of that id. */
FHD_API int32_t FHD_CALL FHD_Forget(uint32_t id);
FHD_API int32_t FHD_CALL FHD_Record(uint32_t id, uint32_t other_id,
   uint32_t event, uint32_t magnitude, uint32_t current_day);
FHD_API int32_t FHD_CALL FHD_Relation(uint32_t id, uint32_t other_id);
/* Existing contact's last event day, 0 if unknown. Pure lookup; the packed
   SAV snapshot and ABI 3 record layout remain unchanged. */
FHD_API uint32_t FHD_CALL FHD_RelationLastDay(uint32_t id,uint32_t other_id);
FHD_API int32_t FHD_CALL FHD_Choose(uint32_t id, const FHDSituation *situation,
   FHDDecision *decision);
/* Seed-aware native paths. Legacy callers can still use the ID-only API. */
int32_t FHD_RecordPeer(uint32_t id, uint32_t other, uint32_t seed, uint32_t event,
                       uint32_t magnitude, uint32_t day);
int32_t FHD_RelationPeer(uint32_t id, uint32_t other, uint32_t seed);
/* Adjust one already-recorded peer incident to the exact next native attitude grade.
   No extra event and no modification of the ABI3 contact/snapshot layout. */
int32_t FHD_AdjustRecordedPeer(uint32_t id,uint32_t other,uint32_t seed,int32_t value,uint32_t day);
uint32_t FHD_RelationPeerLastDay(uint32_t id, uint32_t other, uint32_t seed);
int32_t FHD_ChoosePeer(uint32_t id, uint32_t peer_seed, const FHDSituation *situation,
                       FHDDecision *decision);
typedef struct FHDTradeGoal {
  uint32_t destination, expires, good, quantity, unit_cost;
} FHDTradeGoal;
enum FHDDutyKind {
  FHD_DUTY_SCOUT=1,FHD_DUTY_PATROL,FHD_DUTY_RAID,FHD_DUTY_LINER,
  FHD_DUTY_ENVOY,FHD_DUTY_ESCORT,FHD_DUTY_CLIENT,FHD_DUTY_DISTRESS,FHD_DUTY_REQUEST
};
typedef struct FHDDuty {
  uint32_t kind,star,planet,peer,peer_seed,expires,phase,fee;
} FHDDuty;
int FHD_GetDuty(uint32_t id,FHDDuty *out);
void FHD_SetDuty(uint32_t id,const FHDDuty *duty);
void FHD_NoteFirstStrike(uint32_t id,uint32_t seed,uint32_t player_seed,uint32_t star,
                         uint32_t day,uint32_t side);
uint32_t FHD_FirstStrike(uint32_t id,uint32_t seed,uint32_t player_seed,
                         uint32_t star,uint32_t day);
int32_t FHD_GetTradeGoal(uint32_t id, FHDTradeGoal *goal);
int32_t FHD_TradeExperience(uint32_t id);
void FHD_SetTradeGoal(uint32_t id, const FHDTradeGoal *goal);
unsigned FHD_IncomingCargo(uint32_t destination,unsigned good,uint32_t except_id,uint32_t day);
void FHD_SetTradeOrigin(uint32_t id,uint32_t planet);
uint32_t FHD_TradeOrigin(uint32_t id);
FHD_API uint32_t FHD_CALL FHD_Count(void);
/* Read-only, order-independent audit for exact relationship persistence.
   out[0..5]: active NPC, events, links, positive, negative, checksum. */
FHD_API int32_t FHD_CALL FHD_RelationAudit(uint32_t *out, uint32_t words);
FHD_API void FHD_CALL FHD_AdvanceDay(uint32_t day);
/* No file I/O: game bridge owns save-slot-specific persistence. Query bytes with NULL.
   Returns required/written bytes; negative on error, load is transactional. */
FHD_API int32_t FHD_CALL FHD_Snapshot(void *buffer, uint32_t capacity);
FHD_API int32_t FHD_CALL FHD_Restore(const void *buffer, uint32_t size);
#ifdef __cplusplus
}
#endif
#endif
