from pathlib import Path

MOVIE_MARKER = '// UNIFIED_PROFILE_MOVIE_ROUTING_V1'
TIMING_MARKER = '// UNIFIED_ORIGINAL_MOVIE_TIMING_V1'


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label} anchor not found')
    return text.replace(old, new, 1)


# Route every movie request through the active game's native movie table.
# Fallout 2 retains the stock 17-entry table. Fallout 1 uses its original
# 14-entry table, including script-triggered films, seen-state and save/load.
p = Path('src/game_movie.cc')
s = p.read_text(encoding='utf-8')
if MOVIE_MARKER not in s:
    include_anchor = '#include "touch.h"\n'
    extra = '#include "unified_campaign.h"\n#include "unified_fallout1_movie_profile.h"\n'
    if '#include "unified_campaign.h"\n' not in s:
        s = replace_once(s, include_anchor, include_anchor + extra, 'game_movie.cc include')

    init_old = '''    memset(gGameMoviesSeen, 0, sizeof(gGameMoviesSeen));\n\n    gGameMovieIsPlaying = false;\n'''
    init_new = '''    memset(gGameMoviesSeen, 0, sizeof(gGameMoviesSeen));\n    unifiedFallout1MoviesReset();\n\n    gGameMovieIsPlaying = false;\n'''
    s = replace_once(s, init_old, init_new, 'gameMoviesInit')

    # The same stock reset block occurs once more in gameMoviesReset.
    s = replace_once(s, init_old, init_new, 'gameMoviesReset')

    load_old = '''int gameMoviesLoad(File* stream)\n{\n    if (fileRead(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {\n        return -1;\n    }\n\n    return 0;\n}\n'''
    load_new = '''int gameMoviesLoad(File* stream)\n{\n    // UNIFIED_PROFILE_MOVIE_ROUTING_V1\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        constexpr int count = static_cast<int>(UnifiedFallout1Movie::Count);\n        if (fileRead(gUnifiedFallout1MoviesSeen.data(), sizeof(unsigned char), count, stream) != count) {\n            return -1;\n        }\n        return 0;\n    }\n\n    if (fileRead(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {\n        return -1;\n    }\n\n    return 0;\n}\n'''
    s = replace_once(s, load_old, load_new, 'gameMoviesLoad')

    save_old = '''int gameMoviesSave(File* stream)\n{\n    if (fileWrite(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {\n        return -1;\n    }\n\n    return 0;\n}\n'''
    save_new = '''int gameMoviesSave(File* stream)\n{\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        constexpr int count = static_cast<int>(UnifiedFallout1Movie::Count);\n        if (fileWrite(gUnifiedFallout1MoviesSeen.data(), sizeof(unsigned char), count, stream) != count) {\n            return -1;\n        }\n        return 0;\n    }\n\n    if (fileWrite(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {\n        return -1;\n    }\n\n    return 0;\n}\n'''
    s = replace_once(s, save_old, save_new, 'gameMoviesSave')

    play_old = '''int gameMoviePlay(int movie, int flags)\n{\n    gGameMovieIsPlaying = true;\n'''
    play_new = '''int gameMoviePlay(int movie, int flags)\n{\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        if (!unifiedFallout1MovieIndexIsValid(movie)) {\n            debugPrint("\\nF1 movie index out of range: %d\\n", movie);\n            return -1;\n        }\n        return unifiedFallout1MoviePlay(static_cast<UnifiedFallout1Movie>(movie), flags);\n    }\n\n    gGameMovieIsPlaying = true;\n'''
    s = replace_once(s, play_old, play_new, 'gameMoviePlay')

    seen_old = '''bool gameMovieIsSeen(int movie)\n{\n    return gGameMoviesSeen[movie] == 1;\n}\n\n// 0x44EB14\nbool gameMovieIsPlaying()\n{\n    return gGameMovieIsPlaying;\n}\n'''
    seen_new = '''bool gameMovieIsSeen(int movie)\n{\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        if (!unifiedFallout1MovieIndexIsValid(movie)) {\n            return false;\n        }\n        return unifiedFallout1MovieIsSeen(static_cast<UnifiedFallout1Movie>(movie));\n    }\n    return movie >= 0 && movie < MOVIE_COUNT && gGameMoviesSeen[movie] == 1;\n}\n\n// 0x44EB14\nbool gameMovieIsPlaying()\n{\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        return gUnifiedFallout1MoviePlaying;\n    }\n    return gGameMovieIsPlaying;\n}\n'''
    s = replace_once(s, seen_old, seen_new, 'movie seen/playing')

    p.write_text(s, encoding='utf-8')

# Preserve original new-game movie timing for both halves of the unified game.
# F1: ready room -> Overseer intro (ovrintro.mve) -> V13Ent.
# F2: Elder movie -> stock Fallout 2 starting map. The F1->F2 auto-start must
# not be forced back to Fallout 1 merely because unified mode is enabled.
p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
if TIMING_MARKER not in s:
    condition_old = '''                    if (unifiedCampaignIsEnabled()) {\n                        unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);\n'''
    condition_new = '''                    if (unifiedCampaignIsEnabled()\n                        && unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n                        // UNIFIED_ORIGINAL_MOVIE_TIMING_V1\n                        unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);\n'''
    s = replace_once(s, condition_old, condition_new, 'main.cc unified new-game condition')

    old_f1_start = '''                        // Do NOT play MOVIE_ELDER here. That is Fallout 2's elder\n                        // movie and was the cause of the new-game path looking like\n                        // Fallout 2. Load the actual Fallout 1 campaign start; its\n                        // normal Fallout 1 intro/Overseer scripting remains in\n                        // charge from this point onward.\n                        _main_load_new(mapNameCopy);\n'''
    new_f1_start = '''                        // Fallout 1's original new-game path plays the Overseer\n                        // intro after character selection and before V13Ent. main.cc\n                        // routes MOVIE_ELDER to F1 OverseerIntro while F1 is active,\n                        // so this preserves the original timing without using F2 art.\n                        gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n                        _main_load_new(mapNameCopy);\n'''
    s = replace_once(s, old_f1_start, new_f1_start, 'main.cc F1 Overseer intro')

    p.write_text(s, encoding='utf-8')

print('Unified campaign now preserves original Fallout 1 and Fallout 2 movie routing and timing')
