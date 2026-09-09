from pathlib import Path

p = Path('src/combat.cc')
s = p.read_text(encoding='utf-8')

marker = '// COOP_MELEE_HIT_UNLOCK_V1'
if marker in s:
    print('co-op melee hit unlock already applied')
    raise SystemExit(0)

old = '''        scriptSetObjects(defender->sid, attack->attacker, attack->weapon);\n        _damage_object(defender, attack->defenderDamage, animated, attack->defender != attack->oops, attacker);'''
new = '''        scriptSetObjects(defender->sid, attack->attacker, attack->weapon);\n\n        // COOP_MELEE_HIT_UNLOCK_V1\n        // In realtime co-op a normal melee hit reaction must not become a\n        // prioritized animation lock. Human players remain responsive after\n        // ordinary hits; genuine knockdown/knockout or physical knockback still\n        // uses Fallout's animated damage path and therefore keeps its impact.\n        bool defenderHardReaction = (attack->defenderFlags & (DAM_DEAD | DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0\n            || attack->defenderKnockback > 0;\n        bool defenderAnimated = animated;\n        if (gLocalCoopInitialized\n            && localCoopActorIsHumanOwned(defender)\n            && !defenderHardReaction) {\n            defenderAnimated = false;\n        }\n        _damage_object(defender, attack->defenderDamage, defenderAnimated, attack->defender != attack->oops, attacker);'''

if old not in s:
    raise SystemExit('defender damage application anchor not found')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Applied realtime co-op normal-hit unlock; knockdown/knockback reactions preserved')
