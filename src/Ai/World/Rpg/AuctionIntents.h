/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AUCTIONINTENTS_H
#define PLAYERBOTS_AUCTIONINTENTS_H

#include "Value.h"

class Item;
class PlayerbotAI;

namespace AuctionBot
{
    enum class AuctionIntentState : uint8
    {
        Pending,
        Scheduled,
        Traveling,
        Posting,
        Posted,
        Ignore
    };

    struct AuctionIntent
    {
        ObjectGuid itemGuid;
        uint32 itemEntry = 0;
        uint32 count = 0;
        AuctionIntentState state = AuctionIntentState::Pending;
        uint32 postAttempts = 0;
        time_t createdAt = 0;
        time_t lastPostedAt = 0;
        time_t updatedAt = 0;
    };

    using AuctionIntentList = std::vector<AuctionIntent>;

    class AuctionIntentsValue : public ManualSetValue<AuctionIntentList&>
    {
    public:
        AuctionIntentsValue(PlayerbotAI* botAI, std::string const name = "auction intents");

        std::string const Save() override;
        bool Load(std::string const value) override;
        void Reconcile();

    private:
        AuctionIntentList intents;
    };

    AuctionIntentList& ReconcileAuctionIntents(PlayerbotAI* botAI);
    bool HasPendingAuctionItems(PlayerbotAI* botAI);
    std::vector<Item*> GetPendingAuctionItems(PlayerbotAI* botAI);
    AuctionIntent* GetAuctionIntent(PlayerbotAI* botAI, ObjectGuid itemGuid);
    void MarkAuctionIntentPosted(PlayerbotAI* botAI, ObjectGuid itemGuid);
    void SetAuctionIntentState(PlayerbotAI* botAI, ObjectGuid itemGuid, AuctionIntentState state);
    void RemoveAuctionIntent(PlayerbotAI* botAI, ObjectGuid itemGuid);
}  // namespace AuctionBot

#endif
