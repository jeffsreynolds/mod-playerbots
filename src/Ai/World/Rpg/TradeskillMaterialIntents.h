/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADESKILLMATERIALINTENTS_H
#define PLAYERBOTS_TRADESKILLMATERIALINTENTS_H

#include "Value.h"

class PlayerbotAI;

namespace TradeskillBot
{
    struct TradeskillMaterialIntent
    {
        uint32 itemEntry = 0;
        uint32 requiredCount = 0;
        uint32 ownedCount = 0;
        uint32 deficitCount = 0;
        uint32 maxSpend = 0;
        uint32 sourceSkill = 0;
        uint32 fallbackAttempts = 0;
        time_t nextFallbackAt = 0;
        time_t createdAt = 0;
        time_t updatedAt = 0;
    };

    using TradeskillMaterialIntentList = std::vector<TradeskillMaterialIntent>;

    class TradeskillMaterialIntentsValue : public ManualSetValue<TradeskillMaterialIntentList&>
    {
    public:
        TradeskillMaterialIntentsValue(PlayerbotAI* botAI, std::string const name = "tradeskill material intents");

        std::string const Save() override;
        bool Load(std::string const value) override;

    private:
        TradeskillMaterialIntentList intents;
    };

    TradeskillMaterialIntentList& GetMaterialIntents(PlayerbotAI* botAI);
    void ClearMaterialIntents(PlayerbotAI* botAI);
    void SetMaterialIntentsForSpell(PlayerbotAI* botAI, uint32 learnedSpellId, uint32 sourceSkill);
}  // namespace TradeskillBot

#endif
