/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AuctionValues.h"

#include "AuctionIntents.h"
#include "ItemUsageValue.h"
#include "Playerbots.h"
#include "TravelMgr.h"

namespace
{
    std::string const KEY_VISIT_REQUESTED = "ah_visit_requested";
    std::string const KEY_NEXT_VISIT_LEVEL = "ah_next_visit_level";
    std::string const KEY_NEXT_VISIT_TIME = "ah_next_visit_time";
    std::string const KEY_LAST_VISIT_TIME = "ah_last_visit_time";

    bool IsNearAuctioneer(Player* bot, PlayerbotAI* botAI)
    {
        GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();

        for (ObjectGuid const& guid : npcs)
        {
            Creature* auctioneer = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_AUCTIONEER);
            if (auctioneer && auctioneer->IsInWorld() && auctioneer->IsAlive())
                return true;
        }

        return false;
    }
}  // namespace

namespace AuctionBot
{
    uint32 CalculateNextAuctionLevel(uint32 currentLevel)
    {
        uint32 levelInterval = std::max<uint32>(1, sPlayerbotAIConfig.auctionVisitLevelInterval);
        uint32 remainder = currentLevel % levelInterval;
        if (!remainder)
            return currentLevel;

        return currentLevel + (levelInterval - remainder);
    }

    bool IsVisitRequested(Player* bot)
    {
        return bot && sRandomPlayerbotMgr.GetValue(bot, KEY_VISIT_REQUESTED) > 0;
    }

    bool IsVisitDue(Player* bot)
    {
        if (!bot || !IsVisitRequested(bot))
            return false;

        uint32 nextVisitLevel = sRandomPlayerbotMgr.GetValue(bot, KEY_NEXT_VISIT_LEVEL);
        uint32 nextVisitTime = sRandomPlayerbotMgr.GetValue(bot, KEY_NEXT_VISIT_TIME);

        if (bot->GetLevel() < nextVisitLevel)
            return false;

        if (static_cast<uint32>(time(nullptr)) < nextVisitTime)
            return false;

        return true;
    }

    uint32 GetNextVisitTime(Player* bot)
    {
        return bot ? sRandomPlayerbotMgr.GetValue(bot, KEY_NEXT_VISIT_TIME) : 0;
    }

    uint32 GetLastVisitTime(Player* bot)
    {
        return bot ? sRandomPlayerbotMgr.GetValue(bot, KEY_LAST_VISIT_TIME) : 0;
    }

    void EnsureVisitRequested(Player* bot)
    {
        if (!bot || IsVisitRequested(bot))
            return;

        uint32 now = static_cast<uint32>(time(nullptr));
        uint32 nextLevel = CalculateNextAuctionLevel(bot->GetLevel());
        uint32 nextTime = now + sPlayerbotAIConfig.auctionVisitDelay;
        if (sPlayerbotAIConfig.logAuctionHouseActivity)
            LOG_DEBUG("playerbots.auction",
                      "[VISIT][SCHEDULED] bot={} currentLevel={} nextLevel={} nextTime={}+{}sec reason=first_pending_item",
                      bot->GetName(), bot->GetLevel(), nextLevel, now, sPlayerbotAIConfig.auctionVisitDelay);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_VISIT_REQUESTED, 1);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_LEVEL, nextLevel);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_TIME, nextTime);
    }

    void ClearVisitRequest(Player* bot)
    {
        if (!bot)
            return;

        if (sPlayerbotAIConfig.logAuctionHouseActivity)
            LOG_DEBUG("playerbots.auction", "[VISIT][CLEARED] bot={} reason=no_pending_items", bot->GetName());
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_VISIT_REQUESTED, 0);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_LEVEL, 0);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_TIME, 0);
    }

    void MarkVisitAttempt(Player* bot, uint32 retrySeconds)
    {
        if (!bot)
            return;

        if (!retrySeconds)
            retrySeconds = sPlayerbotAIConfig.auctionVisitRetryDelay;

        uint32 now = static_cast<uint32>(time(nullptr));
        uint32 nextLevel = CalculateNextAuctionLevel(bot->GetLevel());
        uint32 nextTime = now + retrySeconds;
        if (sPlayerbotAIConfig.logAuctionHouseActivity)
            LOG_DEBUG("playerbots.auction",
                      "[VISIT][RETRY_SCHEDULED] bot={} nextLevel={} nextTime={}+{}sec reason=failed_or_incomplete",
                      bot->GetName(), nextLevel, now, retrySeconds);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_VISIT_REQUESTED, 1);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_LAST_VISIT_TIME, now);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_LEVEL, nextLevel);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_TIME, nextTime);
    }

    void MarkVisitComplete(Player* bot, bool hasPendingItems)
    {
        if (!bot)
            return;

        uint32 now = static_cast<uint32>(time(nullptr));
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_LAST_VISIT_TIME, now);

        if (!hasPendingItems)
        {
            if (sPlayerbotAIConfig.logAuctionHouseActivity)
                LOG_DEBUG("playerbots.auction", "[VISIT][COMPLETE] bot={} reason=no_remaining_pending_items",
                          bot->GetName());
            ClearVisitRequest(bot);
            return;
        }

        uint32 nextLevel = CalculateNextAuctionLevel(bot->GetLevel());
        uint32 nextTime = now + sPlayerbotAIConfig.auctionVisitDelay;
        if (sPlayerbotAIConfig.logAuctionHouseActivity)
            LOG_DEBUG("playerbots.auction",
                      "[VISIT][COMPLETE_WITH_PENDING] bot={} nextLevel={} nextTime={}+{}sec remaining_pending_items=true",
                      bot->GetName(), nextLevel, now, sPlayerbotAIConfig.auctionVisitDelay);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_VISIT_REQUESTED, 1);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_LEVEL, nextLevel);
        sRandomPlayerbotMgr.SetPersistentValue(bot, KEY_NEXT_VISIT_TIME, nextTime);
    }
}  // namespace AuctionBot

bool ShouldVisitAuctionHouseValue::Calculate()
{
    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    if (!AuctionBot::HasPendingAuctionItems(botAI))
    {
        AuctionBot::ClearVisitRequest(bot);
        return false;
    }

    AuctionBot::EnsureVisitRequested(bot);

    if (bot->IsInCombat())
        return false;

    if (!WorldPosition(bot).isOverworld())
        return false;

    if (bot->GetGroup() && botAI->HasActivePlayerMaster())
        return false;

    uint32 now = static_cast<uint32>(time(nullptr));
    uint32 lastVisitTime = AuctionBot::GetLastVisitTime(bot);

    if (lastVisitTime && now < (lastVisitTime + sPlayerbotAIConfig.auctionVisitMinInterval))
        return false;

    if (IsNearAuctioneer(bot, botAI))
        return true;

    if (TravelMgr::instance().IsInCity(bot))
        return true;

    if (!AuctionBot::IsVisitDue(bot))
        return false;

    return true;
}
