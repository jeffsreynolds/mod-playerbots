/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADESKILLWORKPLAN_H
#define PLAYERBOTS_TRADESKILLWORKPLAN_H

#include "Value.h"

class PlayerbotAI;

namespace TradeskillBot
{
    struct TradeskillWorkPlan
    {
        uint32 recipeSpellId = 0;
        uint32 learnedSpellId = 0;
        uint32 skillLine = 0;
        uint32 trainerEntry = 0;
        uint32 attemptCount = 0;
        bool pendingTrainerLearn = false;
        time_t lastPlanAt = 0;
        time_t lastAttemptAt = 0;
        time_t nextAttemptAt = 0;
        time_t lastSuccessAt = 0;
    };

    class TradeskillWorkPlanValue : public ManualSetValue<TradeskillWorkPlan&>
    {
    public:
        TradeskillWorkPlanValue(PlayerbotAI* botAI, std::string const name = "tradeskill work plan");

        std::string const Save() override;
        bool Load(std::string const value) override;

    private:
        TradeskillWorkPlan workPlan;
    };

    TradeskillWorkPlan& GetWorkPlan(PlayerbotAI* botAI);
    void ClearTrainerLearnPlan(PlayerbotAI* botAI);
    void SetTrainerLearnPlan(PlayerbotAI* botAI,
                             uint32 trainerEntry,
                             uint32 recipeSpellId,
                             uint32 learnedSpellId,
                             uint32 skillLine,
                             time_t nextAttemptAt);
    bool HasPendingTrainerLearnPlan(PlayerbotAI* botAI);
}  // namespace TradeskillBot

#endif
