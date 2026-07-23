---
name: playerbot-rpg-travel-strategies
description: 'Add Playerbot actions, triggers, strategies, and travel-target integration for location-based activities using existing RPG and TravelMgr systems.'
argument-hint: 'Provide the activity trigger conditions, desired action priorities, and whether the target should be selected by NPC flag or explicit destination resolver.'
user-invocable: true
---

# Playerbot RPG Travel Strategies

## Purpose

Integrate a location-based activity (such as auction-house visits) into Playerbot’s non-combat decision engine by reusing established strategy/trigger/action and TravelTarget lifecycle behavior.

## Use This Skill When

- You are adding a new non-combat strategy with travel and on-arrival behavior.
- You need nearby RPG-NPC checks and interaction gating.
- You need safe registration in shared contexts and random-bot defaults.

## Key Code Anchors (This Repo)

- Action registration: `src/Ai/Base/ActionContext.h`
- Trigger registration: `src/Ai/Base/TriggerContext.h`
- Strategy registration: `src/Ai/Base/StrategyContext.h`
- Strategy examples:
  - `src/Ai/World/Rpg/Strategy/RpgStrategy.cpp`
  - `src/Ai/Base/Strategy/TravelStrategy.cpp`
- Random-bot non-combat composition:
  - `src/Bot/Factory/AiFactory.cpp` (`createNonCombatEngine`)
- Travel selection and target lifecycle:
  - `src/Ai/Base/Actions/ChooseTravelTargetAction.cpp` (`SetNpcFlagTarget`)
  - `src/Mgr/Travel/TravelMgr.h` (`SelectAuctioneerByMap` declared)
  - `src/Mgr/Travel/TravelMgr.cpp` (RPG destination loading; `TravelTarget` lifecycle)
- Nearby RPG target patterns:
  - `src/Ai/Base/Value/PossibleRpgTargetsValue.cpp`
  - `src/Ai/Base/Trigger/RpgTriggers.h`
  - `src/Ai/Base/Trigger/RpgTriggers.cpp`

## Important Repository Reality

`TravelMgr::SelectAuctioneerByMap` is declared but may be unimplemented in this module. If so, either:

1. implement it in `TravelMgr.cpp`, or
2. use existing `SetNpcFlagTarget(UNIT_NPC_FLAG_AUCTIONEER)` logic with faction and distance filtering.

Do not assume the declaration alone is usable.

## Implementation Procedure

1. Add trigger conditions:
   - activity due/opportunistic condition
   - nearby target with pending work
2. Add actions:
   - choose destination action (travel only)
   - perform activity action (no travel planning inside)
3. Register creators in contexts.
4. Add strategy wiring (`InitTriggers`) with priorities:
   - travel selection at normal
   - on-site activity at high
5. Attach strategy to random-bot non-combat defaults/config.

## TravelTarget Integration Rules

- Use `TravelTarget::setTarget` through existing action patterns.
- Avoid custom movement loops.
- Respect existing travel/work state and competing target precedence.

## Nearby NPC Validation Rules

- Candidate must be alive/valid, not hostile, and in interact range.
- Re-resolve nearby NPC before executing the on-site action.
- Abort gracefully if NPC is unavailable.

## Anti-Patterns

- Combining travel selection and posting logic in one action.
- Overriding existing travel target without precedence checks.
- Triggering in combat or urgent group contexts.
- Registering strategy class but forgetting context creators.

## Validation Checklist

- Strategy appears in engine for intended bots.
- Due trigger causes travel target selection.
- Near-target trigger causes activity action only when interactable.
- Existing higher-priority travel is not incorrectly preempted.
- Failures return to pending state without deadlock.
