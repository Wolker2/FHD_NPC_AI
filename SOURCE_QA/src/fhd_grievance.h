#ifndef FHD_GRIEVANCE_H
#define FHD_GRIEVANCE_H
#include <stdint.h>
#define FHD_LOOT_WITNESSES 8u
typedef struct FHDLootWitness {uint32_t id,seed,kind;} FHDLootWitness;
typedef struct FHDLootFrame {
 uint32_t valid,item,star,actor_id,actor_seed,item_id,cost,day,count,kind;
 FHDLootWitness witness[FHD_LOOT_WITNESSES];
} FHDLootFrame;
void FHDNPC_ObservePlayerAttacked(void*player,void*attacker);
void FHDNPC_GrievanceOnAttack(void*victim,void*attacker);
uint32_t FHDNPC_ShouldSuppressReputation(void*player,void*victim);
void FHDNPC_CaptureLootPickup(void*parent,FHDLootFrame*out);
void FHDNPC_CaptureLootShot(void*actor,void*item,FHDLootFrame*out);
void FHDNPC_CompleteLoot(FHDLootFrame*out);
void FHDNPC_OnNativeKill(void*killer,void*victim);
void FHDNPC_GrievanceTick(void*ship,uint32_t kind);
int32_t FHDNPC_ArmGrievance(void);
int32_t FHDNPC_DisarmGrievance(void);
void FHDNPC_ResetGrievance(void);
void FHDNPC_GrievanceStats(uint32_t*out);
#endif
