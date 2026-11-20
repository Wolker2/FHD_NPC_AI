#ifndef FHD_DIALOGUE_H
#define FHD_DIALOGUE_H
#include <stdint.h>
#if defined(_WIN32)
#define FHD_DLG_API __declspec(dllexport)
#else
#define FHD_DLG_API __attribute__((visibility("default")))
#endif
/* 0: stock radio, 1: player events, 2: all eligible local radio. */
enum { FHD_DLG_VANILLA=0, FHD_DLG_PLAYER=1, FHD_DLG_LOCAL=2 };
#define FHD_DIALOGUE_HISTORY_BYTES 64u
enum { FHD_DLG_LOOT_TAKEN=1,FHD_DLG_LOOT_DESTROYED,FHD_DLG_THREAT,
 FHD_DLG_DEMAND,FHD_DLG_ATTACK,FHD_DLG_CONTRACT,FHD_DLG_RIVALRY,
 FHD_DLG_AGREEMENT,FHD_DLG_DELIVERY,FHD_DLG_WARNING,
 FHD_DLG_PIRATE_TAKE,FHD_DLG_PIRATE_DESTROY };
uint32_t FHD_DialogueSender(void);
uint32_t FHD_DialogueTarget(void);
uint32_t FHD_DialoguePersonal(void);
void FHD_DialogueContext(uint32_t player_system,uint32_t day);
int32_t FHD_DialogueNamed(uint32_t event,uint32_t race,uint32_t actor,uint32_t target,
                         uint32_t personal,uint32_t day,uint32_t salt,uint32_t system,
                         const uint16_t *name);
void FHDNPC_LocalDialogue(uint32_t event,const uint8_t *speaker,uint32_t target,
                          uint32_t personal,uint32_t day,uint32_t salt);
const void *FHD_DialogueHistory(void);
void FHD_DialogueRestoreHistory(const uint8_t *bytes);
void FHDNPC_DialogueSettingsLoad(void);
int FHD_SettingsArm(void);
void FHD_SettingsDisarm(void);
void FHDNPC_NewsReset(void);
uint32_t FHDNPC_NewsPump(void);
FHD_DLG_API void FHDNPC_NewsStats(uint32_t *out);
FHD_DLG_API void FHDNPC_InformationStats(uint32_t *out);
/* Caller MUST signal an observed game event, never a predicted outcome.
 * This API creates optional content, NOT a vanilla news/dialogue replacement. */
FHD_DLG_API int32_t FHDNPC_DialogueModeSet(uint32_t mode);
FHD_DLG_API uint32_t FHDNPC_DialogueModeGet(void);
FHD_DLG_API uint32_t FHDNPC_DialogueVariants(void);
FHD_DLG_API uint32_t FHDNPC_DialogueQueued(void);
FHD_DLG_API void FHDNPC_DialogueReset(void);
/* Returns UTF-16 length (no NUL) on success; 0 if queue empty; -required if
 * destination capacity too small. The event is popped ONLY on full success. */
FHD_DLG_API int32_t FHDNPC_DialoguePopW(uint16_t *text,uint32_t capacity);
/* Peek is non-destructive; Acknowledge only after native host accepts the line. */
FHD_DLG_API int32_t FHDNPC_DialoguePeekW(uint16_t *text,uint32_t capacity);
FHD_DLG_API int32_t FHDNPC_DialogueAcknowledge(void);
FHD_DLG_API uint32_t FHDNPC_DialogueStats(uint32_t *out,uint32_t capacity);
#endif
