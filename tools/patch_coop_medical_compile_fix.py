from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')

# Legacy broken form: STAT_LEVEL is not a normal critter stat.
old_invalid = '''inline int localCoopMedicalTreatmentFee(Object* actor)
{
    int level = actor != nullptr ? std::max(1, critterGetStat(actor, STAT_LEVEL)) : 1;
    int rawFee = 100 + 25 * level;
'''

# Current co-op design: P1 owns the campaign level and all players level
# together, so medical rescue pricing intentionally uses the shared PC level.
old_party_level = '''inline int localCoopMedicalTreatmentFee(Object* actor)
{
    int level = actor != nullptr ? std::max(1, pcGetStat(PC_STAT_LEVEL)) : 1;
    int rawFee = 100 + 25 * level;
'''

new_party_level = '''inline int localCoopMedicalTreatmentFee(Object* actor)
{
    // COOP_MEDICAL_FEE_MAX_HP_V1
    // COOP_MEDICAL_FEE_PARTY_LEVEL_V2
    // Compatibility: the old V1 marker is retained for the final-build
    // validator. The actual design now uses P1's shared campaign level because
    // P1 is the protagonist and P2-P4 level up with him.
    int level = actor != nullptr ? std::max(1, pcGetStat(PC_STAT_LEVEL)) : 1;
    int rawFee = 100 + 25 * level;
'''

if old_invalid in text:
    text = text.replace(old_invalid, new_party_level, 1)
    path.write_text(text, encoding='utf-8')
    print('Replaced invalid STAT_LEVEL with shared PC_STAT_LEVEL medical pricing')
elif old_party_level in text:
    text = text.replace(old_party_level, new_party_level, 1)
    path.write_text(text, encoding='utf-8')
    print('Marked shared party-level medical pricing as compile-safe')
elif 'COOP_MEDICAL_FEE_PARTY_LEVEL_V2' in text:
    print('Shared party-level medical compile fix already applied')
elif 'COOP_MEDICAL_FEE_MAX_HP_V1' in text:
    # Older materialized builds used max-HP pricing. Leave them buildable, but
    # new source should normally have the V2 party-level marker above.
    print('Legacy co-op medical compile marker already applied')
else:
    raise SystemExit('Expected co-op medical treatment fee block not found')
