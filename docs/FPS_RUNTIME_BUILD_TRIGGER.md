# FPS runtime build trigger

This user-authored commit follows the live gameplay runtime hook (`COOP_RUNTIME_MAINLOOP_HOOK_V1`) so the pull-request Windows build is triggered by a normal repository write rather than by the materialization bot.

The gameplay source change itself is in `src/main.cc`: `localCoopRuntimeTick()` is called once per live gameplay frame immediately after `inputGetInput()`.
