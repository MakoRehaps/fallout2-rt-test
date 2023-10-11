#ifndef CHARACTER_H
#define CHARACTER_H

class Character {
public:
    int health;
    Character(int h);
    void applyDamage(int damage);
    int getHealth();
};

#endif // CHARACTER_H
