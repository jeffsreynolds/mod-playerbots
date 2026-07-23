/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AUCTIONHOUSEACTIONS_H
#define PLAYERBOTS_AUCTIONHOUSEACTIONS_H

#include "ChooseTravelTargetAction.h"

class ChooseAuctionHouseTargetAction : public ChooseTravelTargetAction
{
public:
    ChooseAuctionHouseTargetAction(PlayerbotAI* botAI, std::string const name = "choose auction house target")
        : ChooseTravelTargetAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuctionPendingItemsAction : public Action
{
public:
    AuctionPendingItemsAction(PlayerbotAI* botAI, std::string const name = "auction pending items")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

class AuctionBuyUpgradesAction : public Action
{
public:
    AuctionBuyUpgradesAction(PlayerbotAI* botAI, std::string const name = "auction buy upgrades")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

#endif
