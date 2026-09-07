# CLAUDE.md

- **Never trust a `*.conf` value:** gitignored `configurationOverrides/*.env` at the server root override it via `AC_*` env vars. Read the effective value with `docker exec ac-worldserver env | grep ^AC_`.

- **Never invent or reject a raid coordinate from a model of the room:** `navprobe` decides the floor, and the trace decides what a derived figure like worst-case clearance is worth. Client data lives in the `ac-client-data` Docker volume — `env/dist/data` is empty on the host and that is not a reason to skip the check. See [docs/engine/pitfalls.md](docs/engine/pitfalls.md).

- **Never read an empty search as proof:** spell behaviour is spread across the DBC, `spell_linked_spell` and `EffectTriggerSpell` chains well below the spellbook entry, so a clean negative usually means the wrong table. Check all three before calling a mechanic impossible. See [docs/engine/pitfalls.md](docs/engine/pitfalls.md).
