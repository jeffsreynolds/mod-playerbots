/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADESKILLMONITORACTION_H
#define PLAYERBOTS_TRADESKILLMONITORACTION_H

#include "Action.h"

class TradeskillMonitorAction : public Action
{
public:
    TradeskillMonitorAction(PlayerbotAI* botAI) : Action(botAI, "tradeskill monitor") {}

    bool Execute(Event event) override;
};

#endif
