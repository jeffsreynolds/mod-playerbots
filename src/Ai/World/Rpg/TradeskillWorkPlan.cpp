/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeskillWorkPlan.h"

#include <atomic>

#include "Playerbots.h"

namespace
{
    std::atomic<uint32> gLegacyTradeskillPlanLoads{0};

    TradeskillBot::TradeskillWorkPlanValue* GetWorkPlanValue(PlayerbotAI* botAI)
    {
        return dynamic_cast<TradeskillBot::TradeskillWorkPlanValue*>(
            botAI->GetAiObjectContext()->GetUntypedValue("tradeskill work plan"));
    }

    std::vector<std::string> SplitCsv(std::string const& text)
    {
        std::vector<std::string> fields;
        std::stringstream stream(text);
        std::string segment;
        while (std::getline(stream, segment, ','))
            fields.push_back(segment);

        return fields;
    }
}  // namespace

namespace TradeskillBot
{
    TradeskillWorkPlanValue::TradeskillWorkPlanValue(PlayerbotAI* botAI, std::string const name)
        : ManualSetValue<TradeskillWorkPlan&>(botAI, workPlan, name)
    {
    }

    std::string const TradeskillWorkPlanValue::Save()
    {
        std::ostringstream out;
        out << value.recipeSpellId << "," << value.learnedSpellId << "," << value.skillLine << ","
            << value.trainerEntry << "," << value.attemptCount << "," << (value.pendingTrainerLearn ? 1 : 0)
            << "," << static_cast<uint64>(value.lastPlanAt) << "," << static_cast<uint64>(value.lastAttemptAt)
            << "," << static_cast<uint64>(value.nextAttemptAt) << "," << static_cast<uint64>(value.lastSuccessAt);
        return out.str();
    }

    bool TradeskillWorkPlanValue::Load(std::string const text)
    {
        value = TradeskillWorkPlan();

        std::vector<std::string> fields = SplitCsv(text);
        if (fields.size() < 4)
            return true;

        try
        {
            // New format (10 fields): recipeSpellId,learnedSpellId,skillLine,trainerEntry,attemptCount,pending,lastPlanAt,lastAttemptAt,nextAttemptAt,lastSuccessAt
            if (fields.size() >= 10)
            {
                value.recipeSpellId = static_cast<uint32>(std::stoul(fields[0]));
                value.learnedSpellId = static_cast<uint32>(std::stoul(fields[1]));
                value.skillLine = static_cast<uint32>(std::stoul(fields[2]));
                value.trainerEntry = static_cast<uint32>(std::stoul(fields[3]));
                value.attemptCount = static_cast<uint32>(std::stoul(fields[4]));
                value.pendingTrainerLearn = std::stoul(fields[5]) > 0;
                value.lastPlanAt = static_cast<time_t>(std::stoull(fields[6]));
                value.lastAttemptAt = static_cast<time_t>(std::stoull(fields[7]));
                value.nextAttemptAt = static_cast<time_t>(std::stoull(fields[8]));
                value.lastSuccessAt = static_cast<time_t>(std::stoull(fields[9]));
            }
            else
            {
                // Legacy format (8 fields): recipeSpellId,trainerEntry,attemptCount,pending,lastPlanAt,lastAttemptAt,nextAttemptAt,lastSuccessAt
                value.recipeSpellId = static_cast<uint32>(std::stoul(fields[0]));
                value.learnedSpellId = value.recipeSpellId;
                value.trainerEntry = static_cast<uint32>(std::stoul(fields[1]));
                value.attemptCount = static_cast<uint32>(std::stoul(fields[2]));
                value.pendingTrainerLearn = std::stoul(fields[3]) > 0;

                if (fields.size() > 4)
                    value.lastPlanAt = static_cast<time_t>(std::stoull(fields[4]));
                if (fields.size() > 5)
                    value.lastAttemptAt = static_cast<time_t>(std::stoull(fields[5]));
                if (fields.size() > 6)
                    value.nextAttemptAt = static_cast<time_t>(std::stoull(fields[6]));
                if (fields.size() > 7)
                    value.lastSuccessAt = static_cast<time_t>(std::stoull(fields[7]));

                uint32 legacyLoads = ++gLegacyTradeskillPlanLoads;
                LOG_DEBUG("playerbots",
                          "[TRADESKILL][PLAN_LOAD_LEGACY] bot={} count={}",
                          bot->GetName(), legacyLoads);
            }
        }
        catch (std::exception const&)
        {
            value = TradeskillWorkPlan();
        }

        return true;
    }

    TradeskillWorkPlan& GetWorkPlan(PlayerbotAI* botAI)
    {
        TradeskillWorkPlanValue* value = GetWorkPlanValue(botAI);
        if (!value)
        {
            static TradeskillWorkPlan empty;
            return empty;
        }

        return value->Get();
    }

    void ClearTrainerLearnPlan(PlayerbotAI* botAI)
    {
        TradeskillWorkPlan& plan = GetWorkPlan(botAI);
        plan.recipeSpellId = 0;
        plan.learnedSpellId = 0;
        plan.skillLine = 0;
        plan.trainerEntry = 0;
        plan.attemptCount = 0;
        plan.pendingTrainerLearn = false;
        plan.nextAttemptAt = 0;
    }

    void SetTrainerLearnPlan(PlayerbotAI* botAI,
                             uint32 trainerEntry,
                             uint32 recipeSpellId,
                             uint32 learnedSpellId,
                             uint32 skillLine,
                             time_t nextAttemptAt)
    {
        TradeskillWorkPlan& plan = GetWorkPlan(botAI);
        plan.trainerEntry = trainerEntry;
        plan.recipeSpellId = recipeSpellId;
        plan.learnedSpellId = learnedSpellId;
        plan.skillLine = skillLine;
        plan.pendingTrainerLearn = recipeSpellId != 0;
        plan.nextAttemptAt = nextAttemptAt;
        plan.lastPlanAt = time(nullptr);
    }

    bool HasPendingTrainerLearnPlan(PlayerbotAI* botAI)
    {
        TradeskillWorkPlan& plan = GetWorkPlan(botAI);
        return plan.pendingTrainerLearn && plan.recipeSpellId != 0;
    }
}  // namespace TradeskillBot
