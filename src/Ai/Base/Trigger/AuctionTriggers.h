/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AUCTIONTRIGGERS_H
#define PLAYERBOTS_AUCTIONTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

class AuctionVisitDueTrigger : public Trigger
{
public:
    AuctionVisitDueTrigger(PlayerbotAI* botAI, std::string const name = "auction visit due")
        : Trigger(botAI, name, 2)
    {
    }

    bool IsActive() override;
};

class NearAuctioneerWithAuctionItemsTrigger : public Trigger
{
public:
    NearAuctioneerWithAuctionItemsTrigger(PlayerbotAI* botAI,
                                          std::string const name = "near auctioneer with auction items")
        : Trigger(botAI, name, 2)
    {
    }

    bool IsActive() override;
};

class NearAuctioneerForUpgradeBuyingTrigger : public Trigger
{
public:
    NearAuctioneerForUpgradeBuyingTrigger(PlayerbotAI* botAI,
                                          std::string const name = "near auctioneer for upgrade buying")
        : Trigger(botAI, name, 2)
    {
    }

    bool IsActive() override;
};

#endif
