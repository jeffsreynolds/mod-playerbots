/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADESKILLPLANNERACTION_H
#define PLAYERBOTS_TRADESKILLPLANNERACTION_H

#include "Action.h"

class TradeskillPlannerAction : public Action
{
public:
    TradeskillPlannerAction(PlayerbotAI* botAI) : Action(botAI, "tradeskill planner") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
