"""2.1.9 exact item-cost compensation, no unrelated game balance edits."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'src/fhd_grievance.c').read_text()
ASM=(ROOT/'src/fhd_grievance_stubs.s').read_text()

def test_item_price_is_captured_before_pickup_or_destruction():
    assert 'f->cost=read32(item,0x20u)' in SRC
    assert 'queue_claim(other,actor,f->item_id,f->day,f->kind,f->cost)' in SRC

def test_fixed_double_cost_and_overflow_guard():
    assert 'return item_cost>0x3fffffffu?0x7fffffffu:item_cost*2u;' in SRC
    assert 'if((int32_t)item_cost<=0)return 0u;' in SRC
    assert 'g_claims[slot].amount=demand' in SRC
    assert 'g_claims[slot].amount=restitution_amount(g_claims[slot].cost)' in SRC
    assert 'cost>0x7fffffffu-old?0x7fffffffu:old+cost' in SRC
    assert 450*2==900

def test_native_response_gets_custom_amount_and_player_handles_payment():
    assert 'FHDNPC_InvokeMoneyDemand((void*)(g_base+MONEY_RESPONSE_RVA)' in SRC
    assert 'target,issuer,&native_response,g_claims[i].amount' in SRC
    assert 'SysFreeString(native_response)' in SRC
    assert '_FHDNPC_InvokeMoneyDemand' in ASM
    assert 'pushl 24(%ebp)' in ASM
    assert 'call *%esi' in ASM
    assert 'FHDNPC_InvokeExtort((void*)(g_base+TRY_EXTORT_RVA)' not in SRC

def test_timing_and_privacy_guards_preserved():
    c=SRC.split('static void process_claim(',1)[1].split('void FHDNPC_GrievanceTick(',1)[0]
    for guard in ['CAN_CONTACT_RVA','if(!player(target)||!same_star(issuer,target))',
                  'if(g_claims[i].last_attempt==day)return;',
                  'EXTORT_COOLDOWN_RVA','DIALOGUE_COUNT_RVA']:
        assert guard in c
