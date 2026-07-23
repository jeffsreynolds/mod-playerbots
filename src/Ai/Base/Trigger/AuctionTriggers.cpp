/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AuctionTriggers.h"

#include "AuctionIntents.h"
#include "ItemUsageValue.h"
#include "Playerbots.h"

namespace
{
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
}

bool AuctionVisitDueTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    if (!AI_VALUE(bool, "should visit auction house"))
        return false;

    if (IsNearAuctioneer(bot, botAI))
        return false;

    TravelTarget* target = AI_VALUE(TravelTarget*, "travel target");
    if (target && target->isActive())
        return false;

    return true;
}

bool NearAuctioneerWithAuctionItemsTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    if (!AuctionBot::HasPendingAuctionItems(botAI))
        return false;

    if (!AI_VALUE(bool, "should visit auction house"))
        return false;

    return IsNearAuctioneer(bot, botAI);
}
