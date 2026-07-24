/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeskillPlannerAction.h"

#include <algorithm>
#include <limits>
#include <set>
#include <unordered_set>

#include "BudgetValues.h"
#include "CraftValue.h"
#include "Event.h"
#include "PlayerbotFactory.h"
#include "PlayerbotSpellRepository.h"
#include "Playerbots.h"
#include "TravelMgr.h"
#include "Trainer.h"
#include "TradeskillMaterialIntents.h"
#include "TradeskillWorkPlan.h"

namespace
{
    constexpr uint32 PLAN_RECHECK_SECONDS = 20 * MINUTE;
    constexpr uint32 PLAN_TRAVEL_RECHECK_SECONDS = 2 * MINUTE;
    constexpr uint32 PLAN_MAX_BACKOFF_SECONDS = 6 * HOUR;

    struct TrainerRecipeCandidate
    {
        uint32 trainerEntry = 0;
        uint32 trainerSpellId = 0;
        uint32 learnedSpellId = 0;
        uint32 skillLine = 0;
        uint32 skillDeltaToGray = 0;
        uint32 cost = 0;
        float travelDistance = std::numeric_limits<float>::max();
        bool hasTenPointWindow = false;
    };

    bool IsEligibleTrainer(Creature* creature, Player* bot)
    {
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            return false;

        Trainer::Trainer* trainer = sObjectMgr->GetTrainer(creature->GetEntry());
        if (!trainer)
            return false;

        if (trainer->GetTrainerType() != Trainer::Type::Tradeskill)
            return false;

        return trainer->IsTrainerValidForPlayer(bot);
    }

    uint32 NextBackoffSeconds(uint32 attemptCount)
    {
        uint32 shift = std::min<uint32>(attemptCount, 6u);
        uint32 delay = 5 * MINUTE * (1u << shift);
        return std::min<uint32>(delay, PLAN_MAX_BACKOFF_SECONDS);
    }

    uint32 GetLearnedSpellId(uint32 trainerSpellId)
    {
        SpellInfo const* trainerSpellInfo = sSpellMgr->GetSpellInfo(trainerSpellId);
        if (!trainerSpellInfo)
            return 0;

        for (SpellEffectInfo const& effect : trainerSpellInfo->GetEffects())
        {
            if (effect.Effect == SPELL_EFFECT_LEARN_SPELL && effect.TriggerSpell)
                return effect.TriggerSpell;
        }

        return trainerSpellId;
    }

    bool IsRecipeCandidate(Player* bot,
                           uint32 learnedSpellId,
                           std::set<uint32> const& reservedReagents,
                           uint32& skillLine,
                           uint32& skillDeltaToGray,
                           bool& hasTenPointWindow)
    {
        skillLine = 0;
        skillDeltaToGray = 0;
        hasTenPointWindow = false;

        SpellInfo const* learnedSpellInfo = sSpellMgr->GetSpellInfo(learnedSpellId);
        if (!learnedSpellInfo)
            return false;

        bool createsItem = false;
        for (SpellEffectInfo const& effect : learnedSpellInfo->GetEffects())
        {
            if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
            {
                createsItem = true;
                break;
            }
        }

        if (!createsItem)
            return false;

        // Do not consume reagents currently reserved for another active craft plan.
        for (uint32 reagent = 0; reagent < MAX_SPELL_REAGENTS; ++reagent)
        {
            int32 reagentEntry = learnedSpellInfo->Reagent[reagent];
            if (reagentEntry <= 0)
                continue;

            if (reservedReagents.find(static_cast<uint32>(reagentEntry)) != reservedReagents.end())
                return false;
        }

        SkillLineAbilityEntry const* skillLineInfo = PlayerbotSpellRepository::Instance().GetSkillLine(learnedSpellId);
        if (!skillLineInfo || !skillLineInfo->SkillLine)
            return false;

        if (!PlayerbotFactory::IsCraftingTradeSkill(static_cast<uint16>(skillLineInfo->SkillLine)))
            return false;

        skillLine = skillLineInfo->SkillLine;
        uint32 currentSkill = bot->GetSkillValue(skillLine);
        uint32 grayAt = skillLineInfo->TrivialSkillLineRankHigh;

        if (grayAt <= currentSkill)
            return false;

        skillDeltaToGray = grayAt - currentSkill;
        hasTenPointWindow = skillDeltaToGray >= 10;
        return true;
    }

    bool PickTrainerRecipePlan(PlayerbotAI* botAI,
                               uint32& trainerEntry,
                               uint32& recipeSpellId,
                               uint32& learnedSpellId,
                               uint32& skillLine)
    {
        Player* bot = botAI->GetBot();
        WorldPosition botPos(bot);
        std::vector<TrainerRecipeCandidate> candidates;
        std::unordered_set<uint32> seenTrainerEntries;
        std::set<uint32> reservedReagents;

        CraftData& craftData = AI_VALUE(CraftData&, "craft");
        if (!craftData.IsEmpty())
        {
            for (auto const& reagent : craftData.required)
            {
                if (reagent.second > 0)
                    reservedReagents.insert(reagent.first);
            }
        }

        std::vector<TravelDestination*> destinations = TravelMgr::instance().getRpgTravelDestinations(bot, true, true);
        for (TravelDestination* destination : destinations)
        {
            if (!destination)
                continue;

            uint32 entry = destination->getEntry() > 0 ? static_cast<uint32>(destination->getEntry()) : 0;
            if (!entry || seenTrainerEntries.find(entry) != seenTrainerEntries.end())
                continue;

            seenTrainerEntries.insert(entry);

            CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(entry);
            if (!cInfo || !(cInfo->npcflag & UNIT_NPC_FLAG_TRAINER))
                continue;

            Trainer::Trainer* trainer = sObjectMgr->GetTrainer(entry);
            if (!trainer || trainer->GetTrainerType() != Trainer::Type::Tradeskill)
                continue;

            if (!trainer->IsTrainerValidForPlayer(bot))
                continue;

            FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->faction);
            ReputationRank reaction = Unit::GetFactionReactionTo(bot->GetFactionTemplateEntry(), factionEntry);
            if (reaction < REP_NEUTRAL)
                continue;

            WorldPosition* trainerPoint = destination->nearestPoint(&botPos);
            float travelDistance = trainerPoint ? botPos.distance(trainerPoint) : std::numeric_limits<float>::max();

            float reputationDiscount = bot->GetReputationPriceDiscount(factionEntry);
            uint32 currentGold = AI_VALUE2(uint32, "free money for", (uint32)NeedMoneyFor::spells);

            for (auto const& spell : trainer->GetSpells())
            {
                Trainer::Spell const* trainerSpell = trainer->GetSpell(spell.SpellId);
                if (!trainerSpell || !trainer->CanTeachSpell(bot, trainerSpell))
                    continue;

                uint32 cost = static_cast<uint32>(floor(trainerSpell->MoneyCost * reputationDiscount));
                if (cost > currentGold)
                    continue;

                uint32 learned = GetLearnedSpellId(trainerSpell->SpellId);
                if (!learned)
                    continue;

                uint32 recipeSkillLine = 0;
                uint32 skillDeltaToGray = 0;
                bool hasTenPointWindow = false;
                if (!IsRecipeCandidate(
                    bot, learned, reservedReagents, recipeSkillLine, skillDeltaToGray, hasTenPointWindow))
                    continue;

                candidates.push_back({entry,
                                      trainerSpell->SpellId,
                                      learned,
                                      recipeSkillLine,
                                      skillDeltaToGray,
                                      cost,
                                      travelDistance,
                                      hasTenPointWindow});
            }
        }

        if (candidates.empty())
            return false;

        std::sort(candidates.begin(), candidates.end(), [](TrainerRecipeCandidate const& left, TrainerRecipeCandidate const& right) {
            if (left.hasTenPointWindow != right.hasTenPointWindow)
                return left.hasTenPointWindow > right.hasTenPointWindow;
            if (left.cost != right.cost)
                return left.cost < right.cost;
            if (left.travelDistance != right.travelDistance)
                return left.travelDistance < right.travelDistance;
            return left.skillDeltaToGray > right.skillDeltaToGray;
        });

        TrainerRecipeCandidate const& selected = candidates.front();
        trainerEntry = selected.trainerEntry;
        recipeSpellId = selected.trainerSpellId;
        learnedSpellId = selected.learnedSpellId;
        skillLine = selected.skillLine;

        LOG_DEBUG("playerbots",
                  "[TRADESKILL][PLAN_SELECT] bot={} trainerEntry={} trainerSpell={} learnedSpell={} skillLine={} "
                  "deltaToGray={} hasTenPointWindow={} cost={}",
                  bot->GetName(), selected.trainerEntry, selected.trainerSpellId, selected.learnedSpellId,
                  selected.skillLine, selected.skillDeltaToGray, selected.hasTenPointWindow ? 1 : 0,
                  selected.cost);

        return true;
    }

    Creature* FindTrainerByEntry(PlayerbotAI* botAI, uint32 trainerEntry)
    {
        if (!trainerEntry)
            return nullptr;

        Player* bot = botAI->GetBot();
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");

        for (ObjectGuid const& guid : npcs)
        {
            Creature* creature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
            if (!creature || creature->GetEntry() != trainerEntry)
                continue;

            if (!IsEligibleTrainer(creature, bot))
                continue;

            return creature;
        }

        return nullptr;
    }

    bool HasLearnedPlanSpell(Player* bot, TradeskillBot::TradeskillWorkPlan const& plan)
    {
        if (plan.learnedSpellId && bot->HasSpell(plan.learnedSpellId))
            return true;

        return plan.recipeSpellId && bot->HasSpell(plan.recipeSpellId);
    }

    bool ScheduleTrainerTravel(PlayerbotAI* botAI, uint32 trainerEntry)
    {
        if (!trainerEntry || !botAI->AllowActivity(TRAVEL_ACTIVITY))
            return false;

        TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
        if (!target)
            return false;

        Player* bot = botAI->GetBot();
        WorldPosition botPos(bot);
        TravelDestination* bestDestination = nullptr;
        WorldPosition* bestPoint = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        for (TravelDestination* destination : TravelMgr::instance().getRpgTravelDestinations(bot, true, true))
        {
            if (!destination || destination->getEntry() != static_cast<int32>(trainerEntry))
                continue;

            WorldPosition* candidatePoint = destination->nearestPoint(&botPos);
            if (!candidatePoint)
                continue;

            float distance = botPos.distance(candidatePoint);
            if (candidatePoint->GetMapId() != botPos.GetMapId())
                distance += 100000.0f;

            if (distance >= bestDistance)
                continue;

            bestDestination = destination;
            bestPoint = candidatePoint;
            bestDistance = distance;
        }

        if (!bestDestination || !bestPoint)
            return false;

        target->setTarget(bestDestination, bestPoint);
        target->setForced(true);
        target->setStatus(TRAVEL_STATUS_TRAVEL);

        LOG_DEBUG("playerbots",
                  "[TRADESKILL][PLAN_TRAVEL] bot={} trainerEntry={} destination={} map={}",
                  bot->GetName(), trainerEntry, bestDestination->getTitle(), bestPoint->GetMapId());

        return true;
    }
}  // namespace

bool TradeskillPlannerAction::Execute(Event /*event*/)
{
    if (!sPlayerbotAIConfig.tradeskillEnabled || !sRandomPlayerbotMgr.IsRandomBot(bot) || botAI->HasActivePlayerMaster())
        return false;

    if (!bot->IsAlive() || bot->IsInCombat())
        return false;

    time_t now = time(nullptr);
    TradeskillBot::TradeskillWorkPlan& plan = AI_VALUE(TradeskillBot::TradeskillWorkPlan&, "tradeskill work plan");

    if (plan.pendingTrainerLearn && HasLearnedPlanSpell(bot, plan))
    {
        LOG_DEBUG("playerbots",
                  "[TRADESKILL][PLAN_COMPLETE] bot={} learnedSpell={} trainerSpell={}",
                  bot->GetName(), plan.learnedSpellId, plan.recipeSpellId);
        TradeskillBot::ClearMaterialIntents(botAI);
        plan.lastSuccessAt = now;
        TradeskillBot::ClearTrainerLearnPlan(botAI);
        return true;
    }

    if (!plan.pendingTrainerLearn)
    {
        if (plan.nextAttemptAt && now < plan.nextAttemptAt)
            return false;

        uint32 trainerEntry = 0;
        uint32 recipeSpellId = 0;
        uint32 learnedSpellId = 0;
        uint32 skillLine = 0;
        if (!PickTrainerRecipePlan(botAI, trainerEntry, recipeSpellId, learnedSpellId, skillLine))
        {
            TradeskillBot::ClearMaterialIntents(botAI);
            plan.lastPlanAt = now;
            plan.nextAttemptAt = now + PLAN_RECHECK_SECONDS;
            return false;
        }

        TradeskillBot::SetTrainerLearnPlan(botAI, trainerEntry, recipeSpellId, learnedSpellId, skillLine, now);
        TradeskillBot::SetMaterialIntentsForSpell(botAI, learnedSpellId, skillLine);
    }

    if (!plan.pendingTrainerLearn || !plan.recipeSpellId)
        return false;

    if (plan.nextAttemptAt && now < plan.nextAttemptAt)
        return false;

    Creature* trainer = FindTrainerByEntry(botAI, plan.trainerEntry);
    if (!trainer)
    {
        if (ScheduleTrainerTravel(botAI, plan.trainerEntry))
        {
            plan.nextAttemptAt = now + PLAN_TRAVEL_RECHECK_SECONDS;
            return true;
        }

        plan.lastAttemptAt = now;
        plan.attemptCount += 1;
        plan.nextAttemptAt = now + NextBackoffSeconds(plan.attemptCount);
        return false;
    }

    Unit* oldTarget = bot->GetSelectedUnit();
    if (!oldTarget || oldTarget->GetGUID() != trainer->GetGUID())
        bot->SetSelection(trainer->GetGUID());

    std::ostringstream learnParam;
    learnParam << "learn " << plan.recipeSpellId;

    LOG_DEBUG("playerbots",
              "[TRADESKILL][PLAN_ATTEMPT] bot={} trainerEntry={} trainerSpell={} learnedSpell={} attempt={}",
              bot->GetName(), plan.trainerEntry, plan.recipeSpellId, plan.learnedSpellId,
              plan.attemptCount + 1);

    bool tried = botAI->DoSpecificAction("trainer", Event("tradeskill planner", learnParam.str().c_str()), true);

    if (oldTarget)
        bot->SetSelection(oldTarget->GetGUID());

    plan.lastAttemptAt = now;
    plan.attemptCount += 1;

    if (HasLearnedPlanSpell(bot, plan))
    {
        LOG_DEBUG("playerbots",
                  "[TRADESKILL][PLAN_COMPLETE] bot={} learnedSpell={} trainerSpell={}",
                  bot->GetName(), plan.learnedSpellId, plan.recipeSpellId);
        TradeskillBot::ClearMaterialIntents(botAI);
        plan.lastSuccessAt = now;
        TradeskillBot::ClearTrainerLearnPlan(botAI);
        return true;
    }

    plan.nextAttemptAt = now + NextBackoffSeconds(plan.attemptCount);
    return tried;
}

bool TradeskillPlannerAction::isUseful()
{
    if (!sPlayerbotAIConfig.tradeskillEnabled || !sRandomPlayerbotMgr.IsRandomBot(bot) || botAI->HasActivePlayerMaster())
        return false;

    if (!bot->IsAlive() || bot->IsInCombat())
        return false;

    TradeskillBot::TradeskillWorkPlan& plan = AI_VALUE(TradeskillBot::TradeskillWorkPlan&, "tradeskill work plan");
    if (plan.pendingTrainerLearn)
        return !plan.nextAttemptAt || plan.nextAttemptAt <= time(nullptr);

    return !plan.nextAttemptAt || plan.nextAttemptAt <= time(nullptr);
}
