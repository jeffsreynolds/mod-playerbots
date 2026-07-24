---
name: tradeskill-economy-governance
description: 'Implement and maintain tradeskill auction governance controls for playerbots: provenance checks, vendor/seeded item blocking, buy-side throttles, sell-side caps, unsold-cycle policy, and profession-cap enforcement.'
argument-hint: 'Reference affected files and say whether you are extending buy controls, sell controls, provenance policy, or unsold-cycle behavior.'
user-invocable: true
---

# Tradeskill Economy Governance

## Purpose

Provide a safe, stable baseline for bot-driven tradeskill economic behavior without bypassing AzerothCore auction transaction safety.

This skill is for governance and enforcement layers, not for full recipe-planning or crafting-session AI.

## When To Use

- You are adding or modifying tradeskill-related auction buy/sell limits.
- You need to enforce provenance requirements for craft-goods before auction listing.
- You need to block unlimited vendor-stock or seeded items from bot auction listings.
- You are tuning unsold-cycle behavior and relist prevention.
- You are integrating new controls into `PlayerbotAIConfig` and `playerbots.conf.dist`.

## Key Files

- `src/PlayerbotAIConfig.h`
- `src/PlayerbotAIConfig.cpp`
- `conf/playerbots.conf.dist`
- `src/Bot/Factory/PlayerbotFactory.cpp`
- `src/Ai/World/Rpg/AuctionIntents.h`
- `src/Ai/World/Rpg/AuctionIntents.cpp`
- `src/Ai/Base/Actions/AuctionHouseActions.cpp`
- `src/Db/PlayerbotSpellRepository.h`
- `src/Db/PlayerbotSpellRepository.cpp`

## Current Governance Surfaces

### Config Controls

Use and preserve these controls when extending behavior:

- `AiPlayerbot.TradeskillEnabled`
- `AiPlayerbot.TradeskillMaxProfessions`
- `AiPlayerbot.TradeskillBuySideControlsEnabled`
- `AiPlayerbot.TradeskillSellSideControlsEnabled`
- `AiPlayerbot.TradeskillRequireProvenance`
- `AiPlayerbot.TradeskillBlockVendorUnlimitedItems`
- `AiPlayerbot.TradeskillSeededItems`
- `AiPlayerbot.TradeskillAuctionReserveMoney`
- `AiPlayerbot.TradeskillAuctionMaxPurchasesPerHour`
- `AiPlayerbot.TradeskillAuctionMaxPurchasesPerDay`
- `AiPlayerbot.TradeskillAuctionMaxSpendPerDay`
- `AiPlayerbot.TradeskillAuctionMaxFactionTransferPerDay`
- `AiPlayerbot.TradeskillAuctionMaxActiveListingsPerItem`
- `AiPlayerbot.TradeskillAuctionMaxActiveListingsPerCategory`
- `AiPlayerbot.TradeskillAuctionMarketFitMaxCompetingListings`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesWhite`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesGreen`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesBlue`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesPurple`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesOrange`
- `AiPlayerbot.TradeskillUnsoldAuctionCyclesYellow`

### Profession Cap Enforcement

The effective random-bot profession cap is enforced in `PlayerbotFactory::InitTradeSkills()` as:

```cpp
min(2, min(TradeskillMaxProfessions, CONFIG_MAX_PRIMARY_TRADE_SKILL))
```

### Auction Intent Metadata

`AuctionIntent` now tracks:

- `postAttempts`
- `lastPostedAt`

Use `MarkAuctionIntentPosted(...)` after successful listing to increment attempts.

### Sell-Side Enforcement

- Block unlimited-vendor items via `PlayerbotSpellRepository::IsItemBuyable(...)`.
- Block configured seeded/generated utility entries.
- Require creator provenance for selected craft-goods classes.
- Enforce per-item and per-category active listing caps.
- Enforce market-fit cap based on competing entry count.
- Enforce unsold-cycle cap by quality before relist.

### Buy-Side Enforcement

- Per-bot hourly/day purchase counters (persisted via `RandomPlayerbotMgr`).
- Per-bot daily spend cap.
- Reserve money floor before spending.
- Faction daily transfer cap (in-memory runtime tracker).

## Invariants (Do Not Break)

- Keep auction posting through:

```cpp
bot->GetSession()->HandleAuctionSellItem(packet);
```

- Keep existing item revalidation checks before posting.
- Never bypass `AuctionIntents` lifecycle when changing list/relist behavior.
- Keep defaults conservative and fail-closed for policy checks.

## Known Limitations

- Faction daily transfer cap is runtime-memory scoped, not persisted across restart.
- Full trainer-driven recipe learning and profession work-planner loops are not part of this governance skill.
- Unsold-cycle vendoring on expiry return is not fully automated yet; current logic prevents relist loops by cap/ignore behavior.

## Procedure

1. Add/adjust settings in `PlayerbotAIConfig` and document in `playerbots.conf.dist`.
2. Add corresponding enforcement checks in `AuctionIntents` and `AuctionHouseActions`.
3. Maintain backward-compatible intent serialization whenever schema changes.
4. Run diagnostics on touched files.
5. Validate runtime behavior with `AiPlayerbot.LogAuctionHouseActivity = 1`.

## Validation Checklist

- A blocked vendor-unlimited or seeded item is never marked as auction intent.
- Craft-goods with missing provenance are rejected when provenance is required.
- Listing attempts stop at configured per-item/per-category/market-fit caps.
- Buy attempts stop at hourly/day count caps and daily spend cap.
- Buy attempts do not reduce money below reserve.
- Profession selection respects both playerbot and worldserver max constraints.
- Existing AH sell transaction path still succeeds for valid items.
