#ifndef FHD_WORLD_H
#define FHD_WORLD_H
#include <stdint.h>
#define FHD_WORLD_V6_BYTES (256u*16u+512u*44u)
#define FHD_WORLD_BYTES (FHD_WORLD_V6_BYTES+64u*24u)
void FHD_WorldReset(void);
unsigned FHD_WorldRisk(uint32_t star,uint32_t day);
void FHD_WorldAttack(uint32_t star,uint32_t day);
void FHD_WorldDelivery(uint32_t star,uint32_t day,unsigned quantity);
void FHD_WorldObserve(uint32_t planet,uint32_t star,uint32_t day,
                      const uint16_t *prices,const uint16_t *needs);
int FHD_WorldQuote(uint32_t planet,uint32_t star,uint32_t day,unsigned good,
                   unsigned *price,unsigned *need);
void FHD_WorldStats(uint32_t day,uint32_t *out);
int FHD_WorldAgree(uint32_t from,uint32_t to,unsigned good,unsigned quantity,uint32_t day);
unsigned FHD_WorldAgreement(uint32_t from,uint32_t to,unsigned good,uint32_t day);
unsigned FHD_WorldFulfil(uint32_t from,uint32_t to,unsigned good,unsigned quantity,uint32_t day);
const void *FHD_WorldBytes(void);
int FHD_WorldValidate(const void *source,unsigned size);
void FHD_WorldLoad(const void *source,unsigned size);
#endif
