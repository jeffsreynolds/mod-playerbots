/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AUCTIONVALUES_H
#define PLAYERBOTS_AUCTIONVALUES_H

#include "Value.h"

class Player;
class PlayerbotAI;

namespace AuctionBot
{
    uint32 CalculateNextAuctionLevel(uint32 currentLevel);
    bool IsVisitRequested(Player* bot);
    bool IsVisitDue(Player* bot);
    uint32 GetNextVisitTime(Player* bot);
    uint32 GetLastVisitTime(Player* bot);
    void EnsureVisitRequested(Player* bot);
    void ClearVisitRequest(Player* bot);
    void MarkVisitAttempt(Player* bot, uint32 retrySeconds = 0);
    void MarkVisitComplete(Player* bot, bool hasPendingItems);
}  // namespace AuctionBot

class ShouldVisitAuctionHouseValue : public BoolCalculatedValue
{
public:
    ShouldVisitAuctionHouseValue(PlayerbotAI* botAI)
        : BoolCalculatedValue(botAI, "should visit auction house", 2 * 15000)
    {
    }

    bool Calculate() override;
};

#endif
