#include <stdint.h>
static uint32_t fake_id;
void FHDNPC_TestSetVerifiedTarget(uint32_t id){fake_id=id;}
void FHDNPC_ObservePartnership(void *ship,uint32_t category){(void)ship;(void)category;}
uint32_t FHDNPC_VerifiedRelationTarget(void *ship,void *target){(void)ship;return target?fake_id:0u;}
uint32_t FHDNPC_VerifiedRelationIdentity(void *ship,void *target,uint32_t *seed){
  *seed=0;
  return FHDNPC_VerifiedRelationTarget(ship,target);
}
uint32_t FHDNPC_ObservedThreat(void *ship,void *target){(void)ship;(void)target;return 50;}
