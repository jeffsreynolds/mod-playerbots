# mod-playerbots Skills Index

This folder contains reusable implementation skills for Playerbots work in this repository.

## Available Skills

### 1) `plan-analysis`
**Use when:** You want to break a plan into section-by-section capabilities, risks, and readiness gates before coding.

**Good prompts:**
- "Map this implementation plan to required skills by section."
- "Assess ownership and prerequisite competencies for this design."

---

### 2) `playerbot-auction-intents`
**Use when:** You are implementing per-item (GUID-based) auction intent tracking, revalidation, deduplication, and cleanup.

**Covers:**
- `ItemUsageValue`-driven selection + instance-level safety
- intent lifecycle/state transitions
- persistence boundaries between AI value serialization and random-bot event metadata

**Good prompts:**
- "Add GUID-based auction intent storage with stale-intent pruning."
- "Implement revalidation before posting pending auction items."

---

### 3) `random-bot-activity-scheduling`
**Use when:** You need durable random-bot activity schedules using level/time gates, cooldowns, deferrals, and retries.

**Covers:**
- due checks (`level + time`)
- minimum intervals and retry timing
- lifecycle persistence and failure-safe scheduling

**Good prompts:**
- "Implement even-level + 30-minute auction visit scheduling."
- "Add deferral policy for combat/group/dungeon contexts."

---

### 4) `playerbot-rpg-travel-strategies`
**Use when:** You are adding actions/triggers/strategies for location-based behavior using RPG and TravelTarget systems.

**Covers:**
- `ActionContext`, `TriggerContext`, `StrategyContext` registration
- `AiFactory::createNonCombatEngine` integration
- travel target selection and near-NPC interaction gating

**Good prompts:**
- "Add an auction visit strategy with due and near-auctioneer triggers."
- "Implement travel-only action plus separate on-site posting action."

---

### 5) `azerothcore-auction-transaction-safety`
**Use when:** You are implementing auction posting and need pinned-core API verification plus transaction safety.

**Covers:**
- validated server interaction/session path
- ownership/deposit/inventory atomicity requirements
- failure handling and anti-patterns

**Good prompts:**
- "Wire bot auction posting through the core-safe interaction path."
- "Define success/failure invariants for auction listing transactions."

---

## Suggested Workflow for the AH Activity Plan

1. `plan-analysis`
2. `playerbot-auction-intents`
3. `random-bot-activity-scheduling`
4. `playerbot-rpg-travel-strategies`
5. `azerothcore-auction-transaction-safety`

## Notes

- Keep responsibilities separated:
  - **eligibility** (`ItemUsageValue` + revalidation)
  - **intent state** (GUID-tracked items)
  - **schedule state** (due/cooldowns/deferrals)
  - **travel strategy** (targeting + triggers)
  - **transaction path** (core-safe posting)
- Prefer conservative defaults and explicit logging around state transitions and failures.
