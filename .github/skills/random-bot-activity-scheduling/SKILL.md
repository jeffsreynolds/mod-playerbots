---
name: random-bot-activity-scheduling
description: 'Implement durable random-bot activity schedules using level/time gates, cooldowns, deferrals, and retry-safe lifecycle behavior.'
argument-hint: 'Provide the activity name, due criteria (time/level), minimum intervals, and persistence target for schedule state.'
user-invocable: true
---

# Random Bot Activity Scheduling

## Purpose

Build durable, low-noise scheduling for random bot activities (like auction visits) that survives normal lifecycle events and respects higher-priority gameplay constraints.

## Use This Skill When

- You need level + time gated visits.
- You must avoid spammy repeated activity attempts.
- You need defer/retry behavior across combat, group obligations, travel, and relog.

## Key Code Anchors (This Repo)

- Random bot state/event APIs:
  - `src/Bot/RandomPlayerbotMgr.h`
  - `src/Bot/RandomPlayerbotMgr.cpp`
- Persistent AI value path:
  - `src/Db/PlayerbotRepository.cpp`
- Random-bot non-combat strategy composition:
  - `src/Bot/Factory/AiFactory.cpp` (`createNonCombatEngine`)

## Schedule Model

```cpp
struct ActivitySchedule
{
    uint32 lastVisitLevel = 0;
    uint32 nextVisitLevel = 0;
    time_t lastVisitTime = 0;
    time_t nextVisitTime = 0;
    bool visitRequested = false;
};
```

## Core Policy

- Due check should require both:
  - `currentLevel >= nextVisitLevel`
  - `now >= nextVisitTime`
- Enforce global cooldowns:
  - minimum between planned visits
  - minimum between any completed visits
- Clear request when no pending work remains.
- Keep pending request on transient failure.

## Deferral Conditions

Defer when any are true:

- In combat.
- In battleground/dungeon context.
- Real-player-led group obligations are active.
- Existing higher-priority travel target should not be preempted.

## Persistence Guidance

- Compact schedule metadata can fit manager event `value/data`.
- Long-lived or richer state should use serializable AI values via repository path.
- Beware implicit expiry semantics if using event-backed state.

## Implementation Procedure

1. Add schedule storage and access helpers (`GetSchedule`, `SetSchedule`, `IsDue`).
2. On first pending work creation, request a visit and set next due level/time.
3. On each decision tick, gate by due + deferral conditions.
4. On successful completion, record `lastVisit*`, clear request or compute next.
5. On failure (travel/posting/NPC unavailable), keep request and push retry time.

## Anti-Patterns

- Triggering immediate travel on every item discovery.
- Using only level or only time gate.
- Resetting schedule entirely on transient failures.
- Ignoring group/master constraints.

## Validation Checklist

- Visit never starts before both due gates are satisfied (unless opportunistic policy says so).
- Cooldowns suppress repeated attempts.
- Failures retry later without losing pending work.
- Relog/restart retains expected schedule state.
- Schedule clears when no pending intents remain.
