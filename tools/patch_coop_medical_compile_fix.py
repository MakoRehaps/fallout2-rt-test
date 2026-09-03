from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')

old = '''inline int localCoopMedicalTreatmentFee(Object* actor)
{
    int level = actor != nullptr ? std::max(1, critterGetStat(actor, STAT_LEVEL)) : 1;
    int rawFee = 100 + 25 * level;
'''
new = '''inline int localCoopMedicalTreatmentFee(Object* actor)
{
    // COOP_MEDICAL_FEE_MAX_HP_V1
    // Fallout's PC_STAT_LEVEL is global to the primary PC, so it cannot price
    // P2-P4 correctly. Scale treatment by the rescued actor's own max HP.
    int maximumHp = actor != nullptr ? std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS)) : 20;
    int rawFee = 100 + 5 * maximumHp;
'''

if old in text:
    text = text.replace(old, new, 1)
    path.write_text(text, encoding='utf-8')
    print('Replaced invalid STAT_LEVEL medical fee with per-actor max-HP scaling')
elif 'COOP_MEDICAL_FEE_MAX_HP_V1' in text:
    print('Co-op medical fee compile fix already applied')
else:
    raise SystemExit('Expected co-op medical treatment fee block not found')
