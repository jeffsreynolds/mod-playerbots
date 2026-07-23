---
title: Playerbot Auction Logging
description: Understand and modify auction-house activity logging for playerbots, including lifecycle traces, filtering, and debug configuration.
tags: [logging, auction, debugging, playerbots, log-categories]
---

# Playerbot Auction Logging

Comprehensive guide to the debug logging system for bot auction-house activity (selling and buying). Covers log points, filtering, configuration, and modification patterns.

## Overview

Auction logging is implemented using AzerothCore's `LOG_DEBUG` macro with a dedicated category `"playerbots.auction"`. This allows server administrators and developers to:
- Track bot auction lifecycle events (scheduling, visits, item placement, buyouts)
- Isolate auction traces from general Playerbots noise using the category filter
- Monitor pricing decisions (AHBot vs fallback)
- Diagnose posting failures and retry behavior

**Log Category:** `"playerbots.auction"`  
**Log Level:** DEBUG  
**Files Modified:**
- [src/Ai/Base/Actions/AuctionHouseActions.cpp](src/Ai/Base/Actions/AuctionHouseActions.cpp) — Sell/buy execution traces
- [src/Ai/World/Rpg/AuctionValues.cpp](src/Ai/World/Rpg/AuctionValues.cpp) — Schedule lifecycle traces
- [src/Ai/World/Rpg/AuctionIntents.cpp](src/Ai/World/Rpg/AuctionIntents.cpp) — Intent creation/removal traces

---

## Log Points & Events

### Selling Path

#### 1. **[SELL][INTENT_MARKED]** — Item added to pending auction list
**Location:** `AuctionIntents.cpp::AuctionIntentsValue::Reconcile()`  
**Triggers:** When an eligible item is first added to the auction intent queue  
**Format:**
```
[SELL][INTENT_MARKED] bot={name} item={entry} guid={guid} count={count} state=pending reason=eligible_item
```
**Example:**
```
[SELL][INTENT_MARKED] bot=Player1 item=3355 guid=0x0400009B2A4A0001 count=1 state=pending reason=eligible_item
```

#### 2. **[VISIT][SCHEDULED]** — AH visit scheduled for first time
**Location:** `AuctionValues.cpp::EnsureVisitRequested()`  
**Triggers:** When a bot obtains its first pending auction item  
**Format:**
```
[VISIT][SCHEDULED] bot={name} currentLevel={level} nextLevel={level} nextTime={now}+{delay}sec reason=first_pending_item
```
**Example:**
```
[VISIT][SCHEDULED] bot=Player1 currentLevel=70 nextLevel=70 nextTime=1721765432+1800sec reason=first_pending_item
```

#### 3. **[SELL][VISIT_EXECUTED]** — Bot arrived at auctioneer, selling begins
**Location:** `AuctionHouseActions.cpp::ChooseAuctionHouseTargetAction::Execute()`  
**Triggers:** When bot reaches auctioneer NPC and begins the visit  
**Format:**
```
[SELL][VISIT_EXECUTED] bot={name} auctioneer_entry={entry} auctioneer_map={map} auctioneer_pos=({x},{y},{z})
```
**Example:**
```
[SELL][VISIT_EXECUTED] bot=Player1 auctioneer_entry=1563 auctioneer_map=0 auctioneer_pos=(10283.456,1654.123,1340.567)
```

#### 4. **[SELL][PLACE_ATTEMPT]** — Attempting to post single item
**Location:** `AuctionHouseActions.cpp::PostItemAtAuctionHouse()`  
**Triggers:** For each item eligible for posting during a visit  
**Format:**
```
[SELL][PLACE_ATTEMPT] bot={name} item={entry} quality={quality} count={count} bid={bid} buyout={buyout} pricingSource={source}
```
**Pricing Sources:**
- `ahbot` — Using AHBot module pricing (if installed)
- `fallback` — Using vendor/item-level-based pricing (when AHBot unavailable)

**Example:**
```
[SELL][PLACE_ATTEMPT] bot=Player1 item=3355 quality=2 count=1 bid=4500 buyout=5000 pricingSource=ahbot
```

#### 5. **[SELL][PLACE_RESULT]** — Outcome of posting attempt
**Location:** `AuctionHouseActions.cpp::PostItemAtAuctionHouse()`  
**Triggers:** After attempting to post (success or failure)  
**Format (Success):**
```
[SELL][PLACE_RESULT] bot={name} item={entry} success=true result=posted inventory_check=passed auction_mgr_lookup=found
```
**Format (Failure):**
```
[SELL][PLACE_RESULT] bot={name} item={entry} success=false result={reason} inventory_check={check_result} auction_mgr_lookup={lookup_result}
```
**Possible Failure Reasons:**
- `deposit_insufficient` — Not enough money for AH deposit
- `inventory_removed_before_post` — Item vanished from inventory during posting
- `auction_not_found_in_mgr` — Auction record not created by AH manager

**Example:**
```
[SELL][PLACE_RESULT] bot=Player1 item=3355 success=true result=posted inventory_check=passed auction_mgr_lookup=found
```

#### 6. **[SELL][PLACE_SKIPPED]** — Item not posted (revalidation failed)
**Location:** `AuctionHouseActions.cpp::PostItemAtAuctionHouse()`  
**Triggers:** When item fails pre-posting validation  
**Format:**
```
[SELL][PLACE_SKIPPED] bot={name} item={entry} reason={reason} quality_threshold={threshold} item_quality={quality}
```
**Possible Skip Reasons:**
- `invalid_quality` — Item quality below configured threshold
- `item_not_tradeable` — Item flags prevent trading
- `price_invalid` — Calculated price is 0 or invalid

**Example:**
```
[SELL][PLACE_SKIPPED] bot=Player1 item=6948 reason=invalid_quality quality_threshold=2 item_quality=1
```

#### 7. **[VISIT][RETRY_SCHEDULED]** — AH visit scheduled for retry
**Location:** `AuctionValues.cpp::MarkVisitAttempt()`  
**Triggers:** When a visit attempt fails or is incomplete  
**Format:**
```
[VISIT][RETRY_SCHEDULED] bot={name} nextLevel={level} nextTime={now}+{delay}sec reason=failed_or_incomplete
```
**Example:**
```
[VISIT][RETRY_SCHEDULED] bot=Player1 nextLevel=70 nextTime=1721765432+1800sec reason=failed_or_incomplete
```

#### 8. **[VISIT][COMPLETE]** — AH visit ended, no more pending items
**Location:** `AuctionValues.cpp::MarkVisitComplete()`  
**Triggers:** When bot finishes visit and auction queue is empty  
**Format:**
```
[VISIT][COMPLETE] bot={name} reason=no_remaining_pending_items
```
**Example:**
```
[VISIT][COMPLETE] bot=Player1 reason=no_remaining_pending_items
```

#### 9. **[VISIT][COMPLETE_WITH_PENDING]** — AH visit ended, more items to post later
**Location:** `AuctionValues.cpp::MarkVisitComplete()`  
**Triggers:** When visit finishes but pending items remain (respecting per-visit limits)  
**Format:**
```
[VISIT][COMPLETE_WITH_PENDING] bot={name} nextLevel={level} nextTime={now}+{delay}sec remaining_pending_items=true
```
**Example:**
```
[VISIT][COMPLETE_WITH_PENDING] bot=Player1 nextLevel=71 nextTime=1721765432+1800sec remaining_pending_items=true
```

#### 10. **[VISIT][CLEARED]** — Schedule cancelled (usually due to log out)
**Location:** `AuctionValues.cpp::ClearVisitRequest()`  
**Triggers:** When pending items are cleared without scheduling a visit  
**Format:**
```
[VISIT][CLEARED] bot={name} reason=no_pending_items
```
**Example:**
```
[VISIT][CLEARED] bot=Player1 reason=no_pending_items
```

### Buying Path

#### 11. **[BUY][VISIT_EXECUTED]** — Bot arrived at auctioneer for buying
**Location:** `AuctionHouseActions.cpp::AuctionBuyUpgradesAction::Execute()`  
**Triggers:** When bot reaches auctioneer and begins scanning for upgrades  
**Format:**
```
[BUY][VISIT_EXECUTED] bot={name} auctioneer_entry={entry} auctioneer_map={map} auctioneer_pos=({x},{y},{z})
```
**Example:**
```
[BUY][VISIT_EXECUTED] bot=Player1 auctioneer_entry=1563 auctioneer_map=0 auctioneer_pos=(10283.456,1654.123,1340.567)
```

#### 12. **[BUY][CONSIDER]** — Auction evaluated as potential upgrade
**Location:** `AuctionHouseActions.cpp::AuctionBuyUpgradesAction::Execute()`  
**Triggers:** For each auction scanned that matches bot's class  
**Format:**
```
[BUY][CONSIDER] bot={name} item={entry} quality={quality} bid={bid} buyout={buyout} usage={usage} isUpgrade={true|false}
```
**Usage Values:** `NEVER`, `NOT_TRADED`, `ITEM_FOR_QUEST`, `SKILL`, `VENDOR_BUY`, `VENDOR_SELL`, `PLAYER_BUY`, `EQUIPMENT`, `JEWELRY`, `AUCTIONED`, `LOOTED_BY_NPC`

**Example:**
```
[BUY][CONSIDER] bot=Player1 item=32370 quality=3 bid=25000 buyout=30000 usage=EQUIPMENT isUpgrade=true
```

#### 13. **[BUY][BID_ATTEMPT]** — Placing bid on auction
**Location:** `AuctionHouseActions.cpp::AuctionBuyUpgradesAction::Execute()`  
**Triggers:** When bot attempts to place a bid (not buyout)  
**Format:**
```
[BUY][BID_ATTEMPT] bot={name} item={entry} bid={amount} current_money={before_bid} cost_estimate={cost}
```
**Example:**
```
[BUY][BID_ATTEMPT] bot=Player1 item=32370 bid=25000 current_money=100000 cost_estimate=25000
```

#### 14. **[BUY][BOUGHT_OUT]** — Direct buyout executed
**Location:** `AuctionHouseActions.cpp::AuctionBuyUpgradesAction::Execute()`  
**Triggers:** When bot executes a buyout (full price, immediate item)  
**Format:**
```
[BUY][BOUGHT_OUT] bot={name} item={entry} buyout_price={amount} current_money={before} money_remaining={after}
```
**Example:**
```
[BUY][BOUGHT_OUT] bot=Player1 item=32370 buyout_price=30000 current_money=100000 money_remaining=70000
```

#### 15. **[BUY][BID_ACCEPTED]** — Bid placed successfully (auction continues)
**Location:** `AuctionHouseActions.cpp::AuctionBuyUpgradesAction::Execute()`  
**Triggers:** After bid is accepted by AH manager  
**Format:**
```
[BUY][BID_ACCEPTED] bot={name} item={entry} bid_amount={amount} money_spent={total} winning_bid={current_winning}
```
**Example:**
```
[BUY][BID_ACCEPTED] bot=Player1 item=32370 bid_amount=25000 money_spent=25000 winning_bid=25000
```

#### 16. **[SELL][INTENT_REMOVED]** — Item removed from auction queue
**Location:** `AuctionIntents.cpp::RemoveAuctionIntent()`  
**Triggers:** When an item is explicitly removed from pending (typically after successful posting or manual cleanup)  
**Format:**
```
[SELL][INTENT_REMOVED] bot={name} item={entry} guid={guid} reason={reason}
```
**Possible Reasons:** `sold`, `cancelled`, `revalidation_failed`, `manual_removal`

**Example:**
```
[SELL][INTENT_REMOVED] bot=Player1 item=3355 guid=0x0400009B2A4A0001 reason=sold
```

---

## Configuration & Enabling

### Server-Side (AzerothCore worldserver.conf)

Add or modify logging configuration:
```conf
# Enable DEBUG level for auction category
LogLevel.playerbots.auction = 2

# Or enable all DEBUG logs (verbose, not recommended for production)
LogLevel = 2
```

### Per-Session Filtering

Most modern AzerothCore versions support filtering:
```bash
# In worldserver console or via RCON
log level debug
log filter playerbots.auction
```

### Module Configuration (playerbots.conf.dist)

Auction logging is always active when DEBUG level is enabled. No separate toggle is needed. However, you can control auction activity itself with:
```conf
# Enable/disable the auction house system entirely
AiPlayerbot.RandomBotAuctionHouse = 1  # 0 to disable

# Adjust frequency and thresholds (affects log volume)
AiPlayerbot.AuctionVisitDelay = 1800        # How often to visit (more visits = more logs)
AiPlayerbot.AuctionItemsPerVisit = 5        # How many items per visit (more items = more [PLACE_ATTEMPT] logs)
```

---

## Modifying Existing Log Points

### Pattern: Adding Context to an Existing Log

**Scenario:** You want to include the item rarity name (not just quality ID) in the `[SELL][PLACE_ATTEMPT]` log.

**File:** `src/Ai/Base/Actions/AuctionHouseActions.cpp`

**Find:**
```cpp
LOG_DEBUG("playerbots.auction",
          "[SELL][PLACE_ATTEMPT] bot={} item={} quality={} count={} bid={} buyout={} pricingSource={}",
          bot->GetName(), item->GetEntry(), item->GetQuality(), item->GetCount(), bid, buyout, 
          ahbotPricing ? "ahbot" : "fallback");
```

**Modify to:**
```cpp
std::string qualityName = "unknown";
if (item->GetQuality() < ITEM_QUALITY_LEGENDARY)
    qualityName = ItemQualities[item->GetQuality()];  // Use AzerothCore's quality name array

LOG_DEBUG("playerbots.auction",
          "[SELL][PLACE_ATTEMPT] bot={} item={} quality={}({}) count={} bid={} buyout={} pricingSource={}",
          bot->GetName(), item->GetEntry(), item->GetQuality(), qualityName, item->GetCount(), bid, buyout, 
          ahbotPricing ? "ahbot" : "fallback");
```

### Pattern: Adding a New Log Point

**Scenario:** You want to log when a bot skips an auctioneer due to distance.

**File:** `src/Ai/Base/Actions/AuctionHouseActions.cpp`

**In ChooseAuctionHouseTargetAction::Execute():**
```cpp
NpcLocation target = TravelMgr::SelectAuctioneerByMap(bot->GetMapId());
if (!target.entry)
{
    LOG_DEBUG("playerbots.auction", 
              "[CHOOSE_AUCTIONEER][FAILED] bot={} map={} reason=no_auctioneer_on_map",
              bot->GetName(), bot->GetMapId());
    return false;
}

float distance = bot->GetDistance(target.x, target.y, target.z);
if (distance > MAX_AUCTIONEER_DISTANCE)
{
    LOG_DEBUG("playerbots.auction",
              "[CHOOSE_AUCTIONEER][TOO_FAR] bot={} auctioneer_entry={} distance={} max_distance={}",
              bot->GetName(), target.entry, (uint32)distance, MAX_AUCTIONEER_DISTANCE);
    return false;
}

LOG_DEBUG("playerbots.auction",
          "[CHOOSE_AUCTIONEER][SELECTED] bot={} auctioneer_entry={} distance={}",
          bot->GetName(), target.entry, (uint32)distance);
return true;
```

---

## Adding New Log Categories

If you want to separate concerns (e.g., intent reconciliation in its own category):

**Pattern:** Replace `"playerbots.auction"` with `"playerbots.auction.intents"`

**Files to Edit:**
- Any LOG_DEBUG in `AuctionIntents.cpp`

**Example:**
```cpp
LOG_DEBUG("playerbots.auction.intents",
          "[RECONCILE] bot={} pending_items={} eligible_items={} reconciled_items={}",
          bot->GetName(), pending.size(), eligibleCount, reconciledCount);
```

**Server Config:**
```conf
LogLevel.playerbots.auction.intents = 2
```

**Filtering:**
```bash
log filter playerbots.auction.intents
```

---

## Troubleshooting Log Issues

### Logs Not Appearing

1. **Check DEBUG level is enabled:**
   ```bash
   log level  # Should show DEBUG (2) or higher
   ```

2. **Verify category is whitelisted (if using filters):**
   ```bash
   log filter  # Should include playerbots.auction or be empty (no filter)
   ```

3. **Ensure auction system is enabled:**
   ```bash
   account set gmlevel <account> 2  # Check config via commands
   ```
   - Verify `AiPlayerbot.RandomBotAuctionHouse = 1` in config

4. **Check server log file location:**
   - Logs written to path specified in `LogsDir` (usually `logs/`)
   - Search for `[SELL]`, `[VISIT]`, `[BUY]` in log file

### Too Many Logs

**Problem:** Log spam makes debugging difficult.

**Solution 1 — Reduce Visit Frequency:**
```conf
AiPlayerbot.AuctionVisitDelay = 3600  # Increase from 1800 (every 1 hour instead of 30 min)
```

**Solution 2 — Reduce Items Per Visit:**
```conf
AiPlayerbot.AuctionItemsPerVisit = 1  # Only post 1 item per visit instead of 5
```

**Solution 3 — Filter to Specific Bot:**
Apply a post-processing grep (not natively supported; requires external log parsing):
```bash
tail -f logs/World.log | grep "bot=PlayerName"
```

### Missing Logs for Specific Event

**Check Execution Path:**
- Verify bot actually reached the relevant code
  - Example: `[SELL][VISIT_EXECUTED]` not appearing? → Bot never reached auctioneer
  - Example: `[BUY][CONSIDER]` not appearing? → Bot not scanning auctions (possibly insufficient funds or class mismatch)

- Add a diagnostic log earlier in the chain:
  ```cpp
  if (shouldPostAuction)
  {
      LOG_DEBUG("playerbots.auction", "[DEBUG] Entering PostItemAtAuctionHouse for bot={}", bot->GetName());
      // ... rest of logic
  }
  ```

---

## Log Message Formatting Best Practices

When adding new logs, follow these conventions:

### 1. **Event Category Prefix**
```
[CATEGORY][SUBCATEGORY] — Always use UPPERCASE
```
- Good: `[SELL][PLACE_ATTEMPT]`
- Bad: `[Sell][Place Attempt]`

### 2. **Bot Identifier**
```
bot={name}  — Always include bot name first
```

### 3. **Relevant IDs**
```
item={entry}  — Use standardized field names
auctioneer_entry={entry}
guid={guid}  — GUID format: 0x0400009B2A4A0001
```

### 4. **State/Result Info**
```
state=pending
success=true
reason=no_remaining_pending_items
```

### 5. **Numeric Values with Context**
```
quality=2 (not just 2)
bid=25000 (include field name)
distance={uint32}distance (cast large numbers)
```

**Complete Example:**
```cpp
LOG_DEBUG("playerbots.auction",
          "[SELL][PLACE_ATTEMPT] bot={} item={} quality={} count={} bid={} buyout={} pricingSource={}",
          bot->GetName(), item->GetEntry(), item->GetQuality(), item->GetCount(), bid, buyout,
          ahbotPricing ? "ahbot" : "fallback");
```

---

## Files & Entry Points

| File | Purpose | Log Points |
|------|---------|-----------|
| [AuctionHouseActions.cpp](src/Ai/Base/Actions/AuctionHouseActions.cpp) | Sell/buy execution | [SELL][VISIT_EXECUTED], [SELL][PLACE_*], [BUY][*] |
| [AuctionValues.cpp](src/Ai/World/Rpg/AuctionValues.cpp) | Schedule lifecycle | [VISIT][SCHEDULED], [VISIT][RETRY_SCHEDULED], [VISIT][COMPLETE*], [VISIT][CLEARED] |
| [AuctionIntents.cpp](src/Ai/World/Rpg/AuctionIntents.cpp) | Intent tracking | [SELL][INTENT_MARKED], [SELL][INTENT_REMOVED] |

---

## Related Skills & Documentation

- [playerbot-auction-intents](./playerbot-auction-intents/SKILL.md) — Intent lifecycle and persistence
- [playerbot-rpg-travel-strategies](./playerbot-rpg-travel-strategies/SKILL.md) — Travel target selection
- [azerothcore-auction-transaction-safety](./azerothcore-auction-transaction-safety/SKILL.md) — Auction transaction safety
- [random-bot-activity-scheduling](./random-bot-activity-scheduling/SKILL.md) — Schedule lifecycle

---

## Quick Reference

### Enable All Auction Logs
```conf
LogLevel.playerbots.auction = 2
```

### Filter Console Output
```bash
log filter playerbots.auction
```

### Common Log Prefixes to Search
| Prefix | Meaning |
|--------|---------|
| `[SELL][INTENT_MARKED]` | Item added to queue |
| `[VISIT][SCHEDULED]` | AH visit scheduled |
| `[SELL][PLACE_ATTEMPT]` | Item posting attempt |
| `[SELL][PLACE_RESULT]` | Posting result (success/fail) |
| `[BUY][CONSIDER]` | Evaluating auction for upgrade |
| `[BUY][BOUGHT_OUT]` | Item purchased via buyout |

