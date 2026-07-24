/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeskillMaterialIntents.h"

#include "Playerbots.h"

namespace
{
    TradeskillBot::TradeskillMaterialIntentsValue* GetMaterialIntentsValue(PlayerbotAI* botAI)
    {
        return dynamic_cast<TradeskillBot::TradeskillMaterialIntentsValue*>(
            botAI->GetAiObjectContext()->GetUntypedValue("tradeskill material intents"));
    }
}

namespace TradeskillBot
{
    TradeskillMaterialIntentsValue::TradeskillMaterialIntentsValue(PlayerbotAI* botAI, std::string const name)
        : ManualSetValue<TradeskillMaterialIntentList&>(botAI, intents, name)
    {
    }

    std::string const TradeskillMaterialIntentsValue::Save()
    {
        std::ostringstream out;
        bool first = true;

        for (TradeskillMaterialIntent const& intent : value)
        {
            if (!first)
                out << "^";
            else
                first = false;

            out << intent.itemEntry << "," << intent.requiredCount << "," << intent.ownedCount << ","
                << intent.deficitCount << "," << intent.maxSpend << "," << intent.sourceSkill << ","
                << intent.fallbackAttempts << "," << static_cast<uint64>(intent.nextFallbackAt) << ","
                << static_cast<uint64>(intent.createdAt) << "," << static_cast<uint64>(intent.updatedAt);
        }

        return out.str();
    }

    bool TradeskillMaterialIntentsValue::Load(std::string const text)
    {
        value.clear();

        for (std::string const& row : split(text, '^'))
        {
            std::vector<std::string> fields = split(row, ',');
            if (fields.size() != 8 && fields.size() != 10)
                continue;

            try
            {
                TradeskillMaterialIntent intent;
                intent.itemEntry = static_cast<uint32>(std::stoul(fields[0]));
                intent.requiredCount = static_cast<uint32>(std::stoul(fields[1]));
                intent.ownedCount = static_cast<uint32>(std::stoul(fields[2]));
                intent.deficitCount = static_cast<uint32>(std::stoul(fields[3]));
                intent.maxSpend = static_cast<uint32>(std::stoul(fields[4]));
                intent.sourceSkill = static_cast<uint32>(std::stoul(fields[5]));
                if (fields.size() == 10)
                {
                    intent.fallbackAttempts = static_cast<uint32>(std::stoul(fields[6]));
                    intent.nextFallbackAt = static_cast<time_t>(std::stoull(fields[7]));
                    intent.createdAt = static_cast<time_t>(std::stoull(fields[8]));
                    intent.updatedAt = static_cast<time_t>(std::stoull(fields[9]));
                }
                else
                {
                    intent.createdAt = static_cast<time_t>(std::stoull(fields[6]));
                    intent.updatedAt = static_cast<time_t>(std::stoull(fields[7]));
                }

                if (!intent.itemEntry || !intent.deficitCount)
                    continue;

                value.push_back(intent);
            }
            catch (std::exception const&)
            {
                continue;
            }
        }

        return true;
    }

    TradeskillMaterialIntentList& GetMaterialIntents(PlayerbotAI* botAI)
    {
        TradeskillMaterialIntentsValue* intentsValue = GetMaterialIntentsValue(botAI);
        if (!intentsValue)
        {
            static TradeskillMaterialIntentList empty;
            return empty;
        }

        return intentsValue->Get();
    }

    void ClearMaterialIntents(PlayerbotAI* botAI)
    {
        GetMaterialIntents(botAI).clear();
    }

    void SetMaterialIntentsForSpell(PlayerbotAI* botAI, uint32 learnedSpellId, uint32 sourceSkill)
    {
        TradeskillMaterialIntentList& intents = GetMaterialIntents(botAI);
        intents.clear();

        Player* bot = botAI->GetBot();
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(learnedSpellId);
        if (!bot || !spellInfo)
            return;

        uint32 now = static_cast<uint32>(time(nullptr));

        for (uint32 reagentIndex = 0; reagentIndex < MAX_SPELL_REAGENTS; ++reagentIndex)
        {
            int32 reagentEntry = spellInfo->Reagent[reagentIndex];
            if (reagentEntry <= 0)
                continue;

            uint32 requiredCount = std::max<int32>(0, spellInfo->ReagentCount[reagentIndex]);
            if (!requiredCount)
                continue;

            uint32 itemEntry = static_cast<uint32>(reagentEntry);
            uint32 ownedCount = AI_VALUE2(uint32, "item count", std::to_string(itemEntry));
            uint32 deficitCount = ownedCount >= requiredCount ? 0 : (requiredCount - ownedCount);
            if (!deficitCount)
                continue;

            uint32 maxSpend = 0;
            if (ItemTemplate const* reagentTemplate = sObjectMgr->GetItemTemplate(itemEntry))
            {
                uint64 buy = static_cast<uint64>(reagentTemplate->BuyPrice) * static_cast<uint64>(deficitCount);
                maxSpend = buy > UINT32_MAX ? UINT32_MAX : static_cast<uint32>(buy);
            }

            intents.push_back(
                {itemEntry, requiredCount, ownedCount, deficitCount, maxSpend, sourceSkill, 0, 0, now, now});
        }

        if (!intents.empty())
        {
            LOG_DEBUG("playerbots",
                      "[TRADESKILL][MATERIAL_INTENTS] bot={} learnedSpell={} sourceSkill={} entries={}",
                      bot->GetName(), learnedSpellId, sourceSkill, intents.size());
        }
    }
}  // namespace TradeskillBot
