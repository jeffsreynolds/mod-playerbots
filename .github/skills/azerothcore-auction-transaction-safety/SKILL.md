---
name: azerothcore-auction-transaction-safety
description: 'Implement auction posting through verified AzerothCore interaction/session paths with correct ownership, deposits, inventory mutation, and failure safety.'
argument-hint: 'Provide the pinned AzerothCore version/branch and the intended posting path (session handler/API), plus required deposit/duration/pricing policy inputs.'
user-invocable: true
---

# AzerothCore Auction Transaction Safety

## Purpose

Ensure bot auction posting uses a core-compatible, transaction-safe server path rather than ad-hoc DB/manager manipulation.

## Use This Skill When

- You are implementing or refactoring auction posting for bots.
- You need inventory-safe, owner-safe auction creation.
- You need compatibility with the pinned AzerothCore core version.

## Key Code Anchors (This Repo)

- Existing safe interaction pattern (vendor sell):
  - `src/Ai/Base/Actions/SellAction.cpp`
- Existing commented auction prototype (do not copy as final):
  - `src/Ai/Base/Actions/LootAction.cpp` (`AuctionItem` block)

## Mandatory First Step

Verify the **exact** auction posting entry point in the pinned AzerothCore source (outside this module if necessary):

- expected session handler/opcode path
- required packet fields (item guid, count, bid/buyout, duration)
- validation/deposit/ownership behaviors
- failure return semantics

Do this before writing bot-side posting code.

## Transaction Safety Requirements

1. On success:
   - auction row/entry created correctly
   - ownership bound to posting bot character
   - deposit calculated/applied correctly
   - item removed from inventory exactly once
2. On failure:
   - item remains in inventory
   - no partial/duplicate auction entries
   - intent remains retryable unless item became invalid

## Separation of Concerns

- Item eligibility stays in item-usage + intent revalidation.
- Pricing lives in a dedicated pricing policy/service.
- Posting action only orchestrates validated listing attempts.

## Implementation Procedure

1. Define `BuildAuctionListing(Item*)` (bid, buyout, duration, stack).
2. Implement `PostItemAtAuctionHouse(...)` via verified core interaction path.
3. Validate nearby auctioneer and interaction context before submit.
4. Revalidate item eligibility immediately before post.
5. Update intent state only after confirmed success.
6. Apply per-visit cap (e.g., 5–10) and set retry timing on failures.

## Anti-Patterns

- Reviving/comment-uncommenting old `LootAction.cpp` auction code as final solution.
- Directly mutating auction DB/manager structures without session-path validation.
- Coupling price decisions to posting transaction logic.
- Marking intent posted before server confirms success.

## Validation Checklist

- Successful post removes item once and creates valid auction ownership/deposit.
- Failed post leaves item untouched and no ghost auction entry exists.
- Batch cap enforced.
- Logs clearly show success/failure reason per attempted GUID.
- Behavior verified against pinned core build after updates.
