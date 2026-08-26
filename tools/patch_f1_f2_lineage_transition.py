from pathlib import Path


def replace_once(path: str, old: str, new: str):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if new in text:
        print(f"{path}: already patched")
        return
    if old not in text:
        raise SystemExit(f"{path}: expected source block not found")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"{path}: patched")


# 1) Inventory inheritance: every captured stack gets one stable 50/50 legacy
# roll. The result is made at the F1->F2 handoff only, then the surviving items
# become ordinary F2 inventory and are saved normally thereafter.
replace_once(
    "src/unified_campaign_carryover.h",
    """    int miscCharges = -1;\n};\n\ninline UnifiedCampaignCarryover gUnifiedCampaignCarryover;\n""",
    """    int miscCharges = -1;\n    uint32_t legacyRoll = 0;\n    bool survivesLegacy = false;\n};\n\ninline UnifiedCampaignCarryover gUnifiedCampaignCarryover;\n\ninline uint32_t unifiedCampaignLegacyItemRoll(int sourcePid, int quantity, int index)\n{\n    // Deliberately deterministic for a captured handoff: re-entering the same\n    // transition cannot reroll an inheritance until a different F1 state is\n    // actually captured. Low bit is the exact 50/50 keep/lost decision.\n    uint32_t value = static_cast<uint32_t>(sourcePid);\n    value ^= static_cast<uint32_t>(quantity) * 0x9E3779B9u;\n    value ^= static_cast<uint32_t>(index + 1) * 0x85EBCA6Bu;\n    value ^= static_cast<uint32_t>(pcGetStat(PC_STAT_LEVEL) + 1) * 0xC2B2AE35u;\n    value ^= value >> 16;\n    value *= 0x7FEB352Du;\n    value ^= value >> 15;\n    value *= 0x846CA68Bu;\n    value ^= value >> 16;\n    return value;\n}\n""",
)

replace_once(
    "src/unified_campaign_carryover.h",
    """        UnifiedCampaignCarryoverItem& captured = carryover.items[carryover.itemCount++];\n        captured.valid = true;\n        std::strncpy(captured.name, name, sizeof(captured.name) - 1);\n""",
    """        int legacyIndex = carryover.itemCount;\n        UnifiedCampaignCarryoverItem& captured = carryover.items[carryover.itemCount++];\n        captured.valid = true;\n        std::strncpy(captured.name, name, sizeof(captured.name) - 1);\n""",
)

replace_once(
    "src/unified_campaign_carryover.h",
    """        captured.equippedFlags = item->flags & OBJECT_EQUIPPED;\n\n        if (captured.itemType == ITEM_TYPE_WEAPON || captured.itemType == ITEM_TYPE_AMMO) {\n""",
    """        captured.equippedFlags = item->flags & OBJECT_EQUIPPED;\n        captured.legacyRoll = unifiedCampaignLegacyItemRoll(\n            captured.sourcePid,\n            captured.quantity,\n            legacyIndex);\n        captured.survivesLegacy = (captured.legacyRoll & 1u) == 0;\n\n        if (captured.itemType == ITEM_TYPE_WEAPON || captured.itemType == ITEM_TYPE_AMMO) {\n""",
)

replace_once(
    "src/unified_campaign_carryover.h",
    """        if (!captured.valid) {\n            continue;\n        }\n\n        Object* restored = unifiedCampaignRestoreOneInventoryItem(captured);\n""",
    """        if (!captured.valid || !captured.survivesLegacy) {\n            continue;\n        }\n\n        Object* restored = unifiedCampaignRestoreOneInventoryItem(captured);\n""",
)

# 2) The unified physical grid ages forward instead of being reset. F1 discovery
# and visited geography become F2 history; temporary content expires during the
# 80-year gap and F2 encounter templates are generated from lineage-derived
# seeds on the same cells.
replace_once(
    "src/unified_world_system.h",
    """inline const UnifiedWorldSystemState& unifiedWorldSystemGetStateConst()\n{\n    unifiedWorldSystemEnsureInitialized();\n    return gUnifiedWorldSystemState;\n}\n\ninline void unifiedWorldSystemAppendLog(\n""",
    """inline const UnifiedWorldSystemState& unifiedWorldSystemGetStateConst()\n{\n    unifiedWorldSystemEnsureInitialized();\n    return gUnifiedWorldSystemState;\n}\n\ninline constexpr uint8_t kUnifiedWorldSystemLineageYears = 80;\n\ninline void unifiedWorldSystemAdvanceFallout1HistoryToFallout2()\n{\n    UnifiedWorldSystemState& state = unifiedWorldSystemGetState();\n    UnifiedWorldSystemWorldState& ancestorWorld = state.worlds[0];\n    UnifiedWorldSystemWorldState& descendantWorld = state.worlds[1];\n\n    // reserved[0] is the lineage generation counter; reserved[1] records the\n    // historical jump in years. This reuses existing serialized bytes so old\n    // COOPMETA.SAV files remain byte-compatible.\n    if (state.reserved[0] < 255) {\n        state.reserved[0]++;\n    }\n    state.reserved[1] = kUnifiedWorldSystemLineageYears;\n\n    for (int cellIndex = 0; cellIndex < kUnifiedWorldSystemCellCount; cellIndex++) {\n        const UnifiedWorldSystemCellState& ancestor = ancestorWorld.cells[cellIndex];\n        UnifiedWorldSystemCellState& descendant = descendantWorld.cells[cellIndex];\n\n        // Geography/exploration is history, not a reset. Temporary battles and\n        // procedural dungeons cannot survive eighty years as the same event.\n        uint8_t historicalFlags = ancestor.flags\n            & (UNIFIED_WORLD_CELL_DISCOVERED | UNIFIED_WORLD_CELL_VISITED);\n        descendant.flags &= ~(UNIFIED_WORLD_CELL_DISCOVERED\n            | UNIFIED_WORLD_CELL_VISITED\n            | UNIFIED_WORLD_CELL_CLEARED\n            | UNIFIED_WORLD_CELL_ACTIVE_EVENT\n            | UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON);\n        descendant.flags |= historicalFlags;\n        descendant.lastVisitGameTime = 0;\n        descendant.temporaryEventExpiry = 0;\n        descendant.temporaryDungeonMapIdx = -1;\n        descendant.templateMapIdx = -1;\n        descendant.chainDepth = 0;\n        descendant.seed = unifiedWorldSystemMixSeed(\n            ancestor.seed\n            ^ 0x4C494E45u // LINE\n            ^ static_cast<uint32_t>(kUnifiedWorldSystemLineageYears)\n            ^ static_cast<uint32_t>(cellIndex * 0x9E3779B9u));\n        descendant.chainLength = static_cast<uint8_t>(\n            1 + descendant.seed % kUnifiedWorldSystemMaxChainMaps);\n        for (int chainIndex = 0; chainIndex < kUnifiedWorldSystemMaxChainMaps; chainIndex++) {\n            descendant.chainMaps[chainIndex] = -1;\n        }\n    }\n\n    // Descendants begin on the same physical portion of the fused continent\n    // reached by their ancestors; F2 content is layered onto that geography.\n    state.travel.currentCellX[1] = state.travel.currentCellX[0];\n    state.travel.currentCellY[1] = state.travel.currentCellY[0];\n    state.travel.selectedCellX[1] = state.travel.currentCellX[0];\n    state.travel.selectedCellY[1] = state.travel.currentCellY[0];\n    state.travel.targetCellX[1] = state.travel.currentCellX[0];\n    state.travel.targetCellY[1] = state.travel.currentCellY[0];\n    state.travel.lastRoadDirection[1] = state.travel.lastRoadDirection[0];\n\n    state.activeChain = UnifiedWorldSystemActiveChain {};\n    state.activeChain.gameId = -1;\n    state.activeChain.currentMapIdx = -1;\n    state.activeChain.encounterTableId = -1;\n    state.activeChain.encounterEntryId = -1;\n    state.revision++;\n}\n\ninline void unifiedWorldSystemAppendLog(\n""",
)

# 3) Make the F1 ending explicitly age the persistent world before the engine
# reboots into the F2 era.
replace_once(
    "src/unified_fallout1_endgame_profile.h",
    """#include \"unified_fallout1_movie_profile.h\"\n""",
    """#include \"unified_fallout1_movie_profile.h\"\n#include \"unified_world_system.h\"\n""",
)

replace_once(
    "src/unified_fallout1_endgame_profile.h",
    """        if (!unifiedCampaignCapturePlayerCarryover(UnifiedGameId::Fallout2)) {\n            unifiedCampaignClearCarryover();\n        }\n\n        // Do not switch cwd/databases under live F1 scripts. The existing main\n""",
    """        if (!unifiedCampaignCapturePlayerCarryover(UnifiedGameId::Fallout2)) {\n            unifiedCampaignClearCarryover();\n        }\n\n        // Fallout 1 is now history, not a second profile the player time-travels\n        // back into. Advance the same physical world eighty years and seed F2's\n        // procedural content from the ancestor-era geography.\n        unifiedWorldSystemAdvanceFallout1HistoryToFallout2();\n\n        // Do not switch cwd/databases under live F1 scripts. The existing main\n""",
)

# 4) Disable the old completed-campaign time-travel switch. After F2 starts the
# F1 era remains historical data in the fused world; there is no postgame jump
# back to a live 2161 runtime.
replace_once(
    "src/unified_campaign_transition.h",
    """inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()\n{\n    if (!unifiedCampaignRequestOtherPostgameWorld()) {\n        return false;\n    }\n\n    gUnifiedCampaignAutoStartNewGame = true;\n    gUnifiedCampaignPostgameResumePending = true;\n    return true;\n}\n""",
    """inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()\n{\n    // The unified campaign is chronological. Fallout 1 becomes ancestor-era\n    // history when Fallout 2 begins; completed eras are not live time-travel\n    // destinations. Post-F2 free roam stays in the descendant era.\n    return false;\n}\n""",
)

print("F1->F2 lineage transition patch complete")
