/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AuctionStrategy.h"

void AuctionStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "auction visit due",
            {
                NextAction("choose auction house target", ACTION_NORMAL)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "near auctioneer with auction items",
            {
                NextAction("auction pending items", ACTION_HIGH),
                NextAction("auction buy upgrades", ACTION_NORMAL)
            }
        )
    );
}
