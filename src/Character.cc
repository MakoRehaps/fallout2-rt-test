
#include "Character.h"

Character::Character(int h) : health(h) {}

void Character::applyDamage(int damage) {
    health -= damage;
}

int Character::getHealth() {
    return health;
}
