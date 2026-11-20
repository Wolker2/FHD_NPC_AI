#ifndef FHD_NATIVE_DUTIES_H
#define FHD_NATIVE_DUTIES_H
#include <stdint.h>
int32_t FHDNPC_ArmDuties(void);
void FHDNPC_DisarmDuties(void);
void FHDNPC_ResetDutyStats(void);
void FHDNPC_DutyStats(uint32_t *out);
void FHDNPC_WorldStats(uint32_t *out);
void FHDNPC_DutyAfter(void *ship,uint32_t category);
void *FHDNPC_TransportDutyDestination(void *ship);
void FHDNPC_TradeDeliveryCompleted(void *ship,void *planet);
int FHDNPC_WaitForEscort(void *ship);
void FHDNPC_TradeCancelled(void *ship);
#endif
