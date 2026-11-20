#ifndef FHD_SAVE_TAG_H
#define FHD_SAVE_TAG_H
#include <stdint.h>
/* V9 sidecar is bound to both the game's stable generation key and exact SAV bytes. */
int32_t FHD_BridgeSaveTaggedSlot(const uint16_t *sav_path, uint64_t galaxy_key,
    const void *snapshot, uint32_t bytes, uint64_t sav_fingerprint);
int32_t FHD_BridgeLoadTaggedSlot(const uint16_t *sav_path, uint64_t galaxy_key,
    uint64_t sav_fingerprint);
int32_t FHD_BridgeMoveTaggedSlotIfMatch(const uint16_t *from,const uint16_t *to, uint64_t destination_sav_tag);
#endif
