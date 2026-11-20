#include "fhd_npc_bridge.h"

/* The bridge has NO executable-address assumptions and NO implicit hooks.
 * Only a verified new-EXE adapter may call its exported procedures. */
static uint32_t g_last_error, g_in_decision;
#define ERR_NONE 0u
#define ERR_INVALID 1u
#define ERR_PROTECTED 2u
#define ERR_NO_EXECUTOR 3u
#define ERR_NESTED 4u
#define ERR_REJECTED 5u

uint32_t FHD_CALL FHD_BridgeVersion(void){return FHD_BRIDGE_ABI;}
void FHD_CALL FHD_BridgeNewGame(void){FHD_BridgeAbortSlot();FHD_Reset();g_in_decision=0;g_last_error=0;}
uint32_t FHD_CALL FHD_BridgeManagedCount(void){return FHD_Count();}
uint32_t FHD_CALL FHD_BridgeLastError(void){return g_last_error;}
static int32_t safe_upsert(const FHDNpcDescriptor *n){
  uint32_t flags;
  if(!n||!n->id||n->id==0xffffffffu){g_last_error=ERR_INVALID;return -1;}
  flags=n->flags;
  /* Crucial: a mod ship with vanilla type number is NOT automatically trusted. */
  if(n->origin==FHD_ORIGIN_UNKNOWN || n->owner==FHD_OWNER_UNKNOWN){
    (void)FHD_Upsert(n->id,n->seed,n->character,n->role,n->native_type,
        flags|FHD_FLAG_STORY_LOCK, n->pirate,n->trade,n->military);
    g_last_error=ERR_PROTECTED;return 0;
  }
  if(n->origin==FHD_ORIGIN_KLISSAN || n->owner==FHD_OWNER_KLISSAN ||
     n->origin==FHD_ORIGIN_DOMINATOR || n->owner==FHD_OWNER_DOMINATORS ||
     n->native_type==FHD_SHIP_DOMINATOR || n->native_type==FHD_SHIP_TRANCLUCATOR ||
     n->native_type==FHD_STATION_DOMINION){
    /* Permanent exclusion: purge an older accidental registration too. */
    (void)FHD_Forget(n->id);
    g_last_error=ERR_PROTECTED;return 0;
  }
  if(n->origin==FHD_ORIGIN_MOD_VERIFIED) flags|=FHD_FLAG_CLASSIFIED;
  if(n->origin>FHD_ORIGIN_KLISSAN || n->owner>FHD_OWNER_KLISSAN){
    g_last_error=ERR_INVALID;return -1;
  }
  /* 13 is a custom station, not a verified built-in type. */
  if(n->origin==FHD_ORIGIN_VANILLA && n->native_type>=13u){
    g_last_error=ERR_PROTECTED;return 0;
  }
  {
    int32_t result=FHD_Upsert(n->id,n->seed,n->character,n->role,n->native_type,
        flags,n->pirate,n->trade,n->military);
    g_last_error=(result==1?ERR_NONE:(result==0?ERR_PROTECTED:ERR_INVALID));
    return result;
  }
}
int32_t FHD_CALL FHD_BridgeUpsert(const FHDNpcDescriptor *npc){return safe_upsert(npc);}
int32_t FHD_CALL FHD_BridgeForget(uint32_t id){return FHD_Forget(id);}
int32_t FHD_CALL FHD_BridgeEvent(const FHDNpcEvent *ev){
  FHDProfile p;
  if(!ev||ev->kind<1u||ev->kind>FHD_EVENT_LOOT_DESTROYED){g_last_error=ERR_INVALID;return -1;}
  if(!FHD_ProfileOf(ev->observer_id,&p)){g_last_error=ERR_PROTECTED;return 0;}
  g_last_error=ERR_NONE;
  return FHD_Record(ev->observer_id,ev->other_id,ev->kind,ev->magnitude,ev->day);
}
void FHD_CALL FHD_BridgeDay(uint32_t day){FHD_AdvanceDay(day);}
int32_t FHD_CALL FHD_BridgeDecide(const FHDNpcDecisionContext *ctx,
  FHDNativeExecutor executor,void *user,FHDDecision *out){
  int32_t status;
  FHDDecision tmp;
  if(!ctx||!out){g_last_error=ERR_INVALID;return -1;}
  out->action=0;out->score=0;out->relation_to_target=0;
  if(g_in_decision){g_last_error=ERR_NESTED;return 0;}
  if(safe_upsert(&ctx->ship)!=1)return 0;
  if(!executor){g_last_error=ERR_NO_EXECUTOR;return 0;}
  g_in_decision=1;
  status=FHD_Choose(ctx->ship.id,&ctx->situation,&tmp);
  if(status!=1||tmp.action==FHD_ACTION_NONE){
    g_in_decision=0;g_last_error=ERR_REJECTED;return 0;
  }
  /* The executor must use unmodified native action methods, without re-issuing
     the original AI decision. NO decision is committed before execution succeeds. */
  status=executor(ctx->ship.id,tmp.action,ctx->situation.target_id,user);
  g_in_decision=0;
  if(status!=1){g_last_error=ERR_REJECTED;return 0;}
  *out=tmp;g_last_error=ERR_NONE;return 1;
}
