---
name: playerbot-auction-intents
description: 'Design and implement GUID-based auction intent tracking for playerbots, including item-usage revalidation, persistence boundaries, and cleanup rules.'
argument-hint: 'Provide the item sources, value type to store intents, and whether persistence should use AI value serialization, random-bot event state, or both.'
user-invocable: true
---

# Playerbot Auction Intents

## Purpose

Implement robust per-item auction intent tracking so random bots can decide what to auction without losing correctness when inventory changes.

## Use This Skill When

- You are adding pending/scheduled/posted intent state for auctionable items.
- You need to map `ITEM_USAGE_AH` to concrete item instances.
- You need duplicate prevention, stale intent cleanup, and revalidation before posting.

## Key Code Anchors (This Repo)

- Item usage decision:
  - `src/Ai/Base/Value/ItemUsageValue.cpp`
  - `src/Ai/Base/Value/ItemUsageValue.h`
  - `src/Ai/Base/ValueContext.h` (`"item usage"` value creator)
- Existing inventory iteration patterns:
  - `src/Mgr/Item/ItemVisitors.h` (`FindItemUsageVisitor`)
  - `src/Ai/Base/Actions/SellAction.cpp` (`SellVendorItemsVisitor`)
- Persistence options:
  - `src/Bot/RandomPlayerbotMgr.h`
  - `src/Bot/RandomPlayerbotMgr.cpp` (`GetValue`, `GetData`, `SetValue`, event internals)
  - `src/Db/PlayerbotRepository.cpp` (AI value save/load path)

## Required Design Rules

1. Track intents by **item GUID**, not just item entry.
2. Keep decision and posting separate:
   - Decision answers: should this item be auctioned?
   - Posting answers: can this item still be auctioned now?
3. Revalidate at action time:
   - Item exists in bag.
   - Still unbound / not BOP-forbidden.
   - Still classified as `ITEM_USAGE_AH`.
   - Not now required by higher-priority needs.
4. Intent creation must be idempotent:
   - Never create duplicate active intents for the same GUID.

## Recommended Data Shape

```cpp
enum class AuctionIntentState
{
    None,
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
    uint32 itemEntry;
    uint32 count;
    AuctionIntentState state;
    time_t createdAt;
    time_t updatedAt;
};
```

## Implementation Procedure

1. Add a value/service that stores intent collection for the bot.
2. Add helper APIs:
   - `HasPendingAuctionItems()`
   - `FindIntentByGuid(ObjectGuid)`
   - `CreateOrUpdateIntent(Item*)`
   - `RemoveIntent(ObjectGuid)`
   - `PruneInvalidIntents()`
3. On loot/inventory-change points, evaluate candidate item with `ItemUsageValue` and create/update intent.
4. On each non-combat cycle (or before posting), run lightweight pruning.
5. Before posting each item, perform strict revalidation and only then transition intent state.

## Persistence Choice Guidance

- Use **AI value serialization** (`PlayerbotRepository`) for larger/long-lived intent sets.
- Use `RandomPlayerbotMgr` event `value/data` for compact schedule metadata, not large lists.
- If both are used, treat AI value as source of truth for intents.

## Anti-Patterns

- Storing only item entry ID and posting any matching item.
- Posting without rechecking binding/usage/state.
- Treating `ITEM_USAGE_AH` as permanent after initial decision.
- Keeping stale intents forever after item loss, trade, consume, or equip.

## Validation Checklist

- Intent created once per GUID.
- Intent removed when item disappears.
- Intent transitions are monotonic and recoverable.
- Revalidation blocks posting when item became ineligible.
- Restart/load preserves pending intent set (if chosen as persistent).
