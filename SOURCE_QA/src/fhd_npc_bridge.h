#ifndef FHD_NPC_BRIDGE_H
#define FHD_NPC_BRIDGE_H
#include <stdint.h>
#include "fhd_npc_ai.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FHD_BRIDGE_ABI 5
/* Game adapter MUST establish provenance; never infer mod/klissan owner from type alone. */
#define FHD_ORIGIN_UNKNOWN 0u
#define FHD_ORIGIN_VANILLA 1u
#define FHD_ORIGIN_MOD_VERIFIED 2u
#define FHD_ORIGIN_DOMINATOR 3u
#define FHD_ORIGIN_KLISSAN 4u
/* Not all station types should make ship AI decisions. */
#define FHD_OWNER_UNKNOWN 0u
#define FHD_OWNER_COALITION 1u
#define FHD_OWNER_PIRATES 2u
#define FHD_OWNER_DOMINATORS 3u
#define FHD_OWNER_KLISSAN 4u

/* Zero=pass to vanilla, one=accepted native decision, negative=failure. */
typedef int32_t (FHD_CALL *FHDNativeExecutor)(
   uint32_t npc_id, uint32_t action, uint32_t target_id, void *opaque);

#pragma pack(push,1)
typedef struct FHDNpcDescriptor {
  uint32_t id, seed, native_type, flags;
  uint32_t origin, owner, role;
  int32_t character; /* -1: classify by original career */
  uint32_t pirate, trade, military;
} FHDNpcDescriptor;
typedef struct FHDNpcDecisionContext {
  FHDNpcDescriptor ship;
  FHDSituation situation;
} FHDNpcDecisionContext;
typedef struct FHDNpcEvent {
  uint32_t observer_id, other_id, kind, magnitude, day;
} FHDNpcEvent;
#pragma pack(pop)

FHD_API uint32_t FHD_CALL FHD_BridgeVersion(void);
FHD_API void FHD_CALL FHD_BridgeNewGame(void);
FHD_API uint32_t FHD_CALL FHD_BridgeManagedCount(void);
/* Ship life cycle. Unknown/unverified/hostile class is never controlled. */
FHD_API int32_t FHD_CALL FHD_BridgeUpsert(const FHDNpcDescriptor *npc);
FHD_API int32_t FHD_CALL FHD_BridgeForget(uint32_t npc_id);
/* Call on gameplay events only after vanilla action succeeds. */
FHD_API int32_t FHD_CALL FHD_BridgeEvent(const FHDNpcEvent *event);
FHD_API void FHD_CALL FHD_BridgeDay(uint32_t day);
/* Host must call only at ONE verified decision point (not the entire NextDayLogic).
   Returns 1 if execution succeeded and that original decision must be suppressed;
   returns 0 to run the entire original decision branch untouched. */
FHD_API int32_t FHD_CALL FHD_BridgeDecide(const FHDNpcDecisionContext *ctx,
   FHDNativeExecutor executor, void *user, FHDDecision *decision);
/* Call AFTER game save completed. Uses atomic sidecar tied to its path and galaxy key.
   On load failure memory is RESET, not leaked from a previously played save.
   UTF-16 path is an exact SAV filename, never a directory. */
FHD_API int32_t FHD_CALL FHD_BridgeSaveSlot(const uint16_t *sav_path, uint64_t galaxy_key);
FHD_API int32_t FHD_CALL FHD_BridgeLoadSlot(const uint16_t *sav_path, uint64_t galaxy_key);
/* Async-save-safe transaction: PREPARE at QueueSave (snapshot before game mutates),
   COMMIT only after TSaver.Execute actually installs the SAV successfully,
   ABORT on write failure. Never call COMMIT from SaveGameToFile's boolean. */
FHD_API int32_t FHD_CALL FHD_BridgePrepareSlot(const uint16_t *sav_path, uint64_t galaxy_key);
FHD_API int32_t FHD_CALL FHD_BridgeCommitSlot(const uint16_t *sav_path, uint64_t galaxy_key);
FHD_API void FHD_CALL FHD_BridgeAbortSlot(void);
/* Mirror an already-confirmed SAV slot move/rotation; call only AFTER MoveFileW succeeds. */
FHD_API int32_t FHD_CALL FHD_BridgeMoveSlot(const uint16_t *from_sav, const uint16_t *to_sav);
FHD_API uint32_t FHD_CALL FHD_BridgeLastError(void);
#ifdef __cplusplus
}
#endif
#endif
