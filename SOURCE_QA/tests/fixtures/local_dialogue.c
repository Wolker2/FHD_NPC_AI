#include "../../src/fhd_dialogue.h"

const uint8_t *fhd_test_player_star;
unsigned fhd_test_dialogue_calls;
void FHDNPC_LocalDialogue(uint32_t event,const uint8_t *speaker,uint32_t target,
                          uint32_t personal,uint32_t day,uint32_t salt){
 fhd_test_dialogue_calls++;
 const uint8_t *star=*(const uint8_t *const *)(speaker+0x24u);
 if(!star||star!=fhd_test_player_star)return;
 uint32_t system=*(const uint32_t *)(star+4u);
 FHD_DialogueContext(system,day);
 (void)FHD_DialogueNamed(event,speaker[0x408u],*(const uint32_t *)(speaker+4u),
      target,personal,day,salt,system,*(const uint16_t *const *)(speaker+8u));
}
