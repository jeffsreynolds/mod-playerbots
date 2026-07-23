---
name: plan-analysis
description: 'Analyze implementation plans and determine required skills per section. Use when user asks to break a plan into capabilities, competencies, ownership, or prerequisites before coding.'
argument-hint: 'Paste the plan text or reference the plan file path to map each section to required skills.'
user-invocable: true
---

# Plan Analysis

## Purpose

Map each section of an implementation plan to the concrete skills needed to execute it.

Use this skill to transform a high-level plan into an actionable capability matrix before implementation starts.

## When To Use

- The user asks to analyze a plan.
- The user asks what skills are needed for each section of a plan.
- The user asks for competencies, prerequisites, or ownership mapping.
- The user wants a delivery-readiness review before coding.

## Inputs

- Plan text or a plan file path.
- Optional context:
  - Codebase architecture summary.
  - Team constraints.
  - Preferred stack and tools.

## Procedure

1. Parse the plan into numbered sections.
2. For each section, identify required skills in these categories:
   - Domain skill: game/server/domain knowledge needed.
   - Code skill: language/framework/components to modify.
   - System skill: architecture, lifecycle, and integration points.
   - Data skill: schema/state/persistence requirements.
   - Ops skill: testing, observability, rollout, and risk controls.
3. Classify each skill with:
   - Priority: critical, important, optional.
   - Proficiency level: baseline, intermediate, expert.
   - Evidence: file, subsystem, or dependency that justifies the skill.
4. Produce section-level risks:
   - Missing skill risks.
   - Coupling risks.
   - Sequencing risks.
5. Produce a final execution map:
   - Skill checklist by section.
   - Recommended implementation order.
   - Validation gates per section.

## Output Format

Use this exact shape:

```markdown
## Section N: <section title>
Required skills:
- <skill name> (priority: <critical|important|optional>, level: <baseline|intermediate|expert>)
- <skill name> (priority: <critical|important|optional>, level: <baseline|intermediate|expert>)

Evidence:
- <why this skill is needed>

Risks:
- <risk if skill is missing>

Validation gate:
- <how to verify section is implementation-ready>
```

Then provide:

```markdown
## Cross-Section Summary
- Critical skills across all sections:
- Skills that can be deferred:
- Suggested implementation sequence:
- Suggested owners by skill cluster:
```

## Heuristics

- Prefer concrete, implementation-relevant skill names over generic labels.
- Tie every skill to direct evidence from the plan and codebase context.
- If a section lacks enough detail, mark assumptions explicitly.
- Keep skill granularity at the level of tasks that can be assigned.

## Quality Bar

A good analysis must:

- Cover every section in the input plan.
- Include at least one validation gate per section.
- Distinguish critical vs optional skills.
- Surface at least one meaningful risk where applicable.
- End with a clear execution sequence.
