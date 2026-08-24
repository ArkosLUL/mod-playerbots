# mod-playerbots documentation

Durable knowledge about how the bot AI works: design decisions and why they were made, traps that
have already cost a debugging session, verified encounter facts, and known-open gaps. It is not a
changelog — git history covers that.

## Where things live

| Directory | Holds |
|---|---|
| [engine/](engine/) | How the bot picks an ability, the trap catalogue, and cross-encounter lessons |
| [systems/](systems/) | Cross-cutting subsystems: itemization, loot, gear maintenance, consumables and burst |
| [classes/](classes/) | Per-class rotation state: current ladders, decisions, open gaps |
| [raids/](raids/) | Per-raid encounter knowledge, plus shared raid conventions |
| [plans/](plans/) | In-flight work only — see below |

**Start with [engine/pitfalls.md](engine/pitfalls.md)** if something silently does nothing. Most
failures in this codebase produce no error at all. Before building a boss strategy — above all one
with vehicles, formation movement, or twenty-five bots asking the same question — read
[engine/raid-mechanics-lessons.md](engine/raid-mechanics-lessons.md).

## Keeping it that way

A plan lives in `plans/` **only while the work is in flight**. When it ships, its durable content
moves into the matching `engine/` / `systems/` / `classes/` / `raids/` doc and the plan is deleted.
A finished plan left behind is how this tree accumulated 72 files describing work that had already
merged.

Use `plans/<slug>/<slug>.PLAN.md`; prefix the ticket id if there is one.

When you write into a permanent doc, keep:

- verified facts — spell, item and NPC ids, timers, thresholds, config names with their **shipped**
  defaults;
- decisions **with their rationale**, including "this is not a defect, do not re-audit" records;
- traps and silent-failure modes;
- known-open gaps.

Leave out file-by-file change lists, pasteable code, step ordering, and verification checklists.
State each shared fact once, in `engine/` or `systems/`, and link to it from the class or raid doc
rather than restating it.
