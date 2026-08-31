# Camera + class selection build trigger

This user-authored commit triggers a fresh Windows co-op build after:

- late-frame Ascent-style shared camera ownership was materialized into `src/main.cc`, and
- per-player ready-room archetype selection was materialized into `src/local_coop_group_room.h`.

Build #864 predates the late-frame camera fix and must not be used to validate camera following.
