#!/usr/bin/env python3
"""Build `full-v12.ndjson`: one small pull carrying every record type the recorder can write.

`coverage-v12.ndjson` holds only the six records the coverage view needs, so ten of the sixteen
invariant checks iterate an empty list against it and pass without testing anything. This one has
combat, movement, probes, hazards and a death in it, so the checks and every renderer meet real rows.

Generated rather than hand-written because the invariants are arithmetic - a damage row has to agree
with the health trajectory, cumulative damage dealt has to rise, a killing blow has to have a damage
row behind it - and keeping that consistent by hand is how a fixture ends up proving nothing.
"""
from __future__ import annotations

import json
import pathlib

TANK, HEAL, RANGED, HUMAN = 5001, 5002, 5003, 5004
PET = 5005
BOSS = 4294967400
ADD = 4294967401

PLAYERS = {
    TANK: ("Bulwark", "tank", "warrior", 0),
    HEAL: ("Mercy", "heal", "priest", 0),
    RANGED: ("Trueshot", "ranged", "hunter", 0),
    HUMAN: ("Arkos", "melee", "rogue", 1),
}
MAXHP = {TANK: 50000, HEAL: 30000, RANGED: 32000, HUMAN: 34000, PET: 8000,
         BOSS: 2000000, ADD: 90000}

SPELLS = {
    100: "Shadow Bolt", 101: "Flash Heal", 102: "Power Word: Shield",
    103: "Rend", 104: "Cleave", 105: "Consuming Darkness",
}

records: list[dict] = []


def add(**fields):
    records.append(fields)


add(e="hdr", v=12, ts=1789500000000, map=603, inst=4, diff=1, boss="fixtureboss",
    bin=1789499000000,
    cfg={"cheats": "", "mapthreads": 2, "hardmode": {"fixtureboss": False}},
    roster=[{"g": guid, "n": name, "r": role, "c": klass, "h": human}
            for guid, (name, role, klass, human) in PLAYERS.items()])

for guid, (name, role, klass, human) in PLAYERS.items():
    add(t=1, e="unit", g=guid, en=0, n=name, lvl=80, mhp=MAXHP[guid], b=0, c=klass, r=role, h=human)
add(t=1, e="unit", g=PET, en=416, n="Ruirin", lvl=80, mhp=MAXHP[PET], b=0, own=RANGED)
add(t=1, e="unit", g=BOSS, en=33999, n="Fixture Boss", lvl=83, mhp=MAXHP[BOSS], b=1)
add(t=1, e="unit", g=ADD, en=33998, n="Fixture Add", lvl=82, mhp=MAXHP[ADD], b=0)

for spell, name in SPELLS.items():
    add(t=1, e="spell", sp=spell, n=name)

add(t=2, e="covdef", d=[
    [1, "fixture tank trigger", "fixture", "combat", ""],
    [2, "fixture heal trigger", "fixture", "combat", ""],
    [3, "fixture dodge trigger", "fixture", "combat", "dodge"],
])
add(t=0, e="pull", boss="fixtureboss", src="bossstate")

# Health trajectories. dmg.hp is the health the hit landed *on*, so each row carries the percentage
# before its own amount is taken off - the invariant that caught a whole class of misreading.
health = {guid: float(MAXHP[guid]) for guid in list(PLAYERS) + [PET, ADD, BOSS]}
dealt = {guid: 0 for guid in list(PLAYERS) + [PET]}


def pct(guid) -> float:
    return round(max(0.0, health[guid]) * 100.0 / MAXHP[guid], 2)


def hurt(when, source, target, spell, amount, lethal=False):
    before = pct(target)
    health[target] -= amount
    add(t=when, e="dmg", s=source, d=target, sp=spell, a=amount,
        ok=max(0, -int(health[target])) if lethal else 0,
        sc=1, ab=0, rs=0, hp=before)
    if source in dealt:
        dealt[source] += amount


def mend(when, source, target, spell, amount):
    health[target] = min(float(MAXHP[target]), health[target] + amount)
    add(t=when, e="heal", s=source, d=target, sp=spell, a=amount, oh=0, hp=pct(target))


def snapshot(when, targets=None, hazard=False):
    targets = targets or {}
    rows = []
    for guid in (TANK, HEAL, RANGED, HUMAN, PET, BOSS, ADD):
        if health[guid] <= 0:
            continue
        spot = SPOTS[guid]
        rows.append([guid, spot[0], spot[1], spot[2], 0.5, pct(guid), 100.0,
                     targets.get(guid, 0), 0, 0, 0,
                     dealt.get(guid, 0)])
    row = {"t": when, "e": "snap", "u": rows}
    if hazard:
        row["hz"] = [[105, 12.0, 0.0, 0.0, 6.0, 1]]
    records.append(row)


SPOTS = {
    TANK: (2.0, 0.0, 0.0), HEAL: (-18.0, 4.0, 0.0), RANGED: (-20.0, -3.0, 0.0),
    HUMAN: (3.0, 1.0, 0.0), PET: (1.0, 2.0, 0.0), BOSS: (0.0, 0.0, 0.0),
    ADD: (14.0, 0.0, 0.0),
}

snapshot(-2000)
add(t=-1000, e="note", g=TANK, k="fixture.phase", txt="1")
add(t=-1000, e="note", g=TANK, k="fixture.role", txt="anchor")

snapshot(0, {BOSS: TANK, ADD: RANGED})
add(t=100, e="act", g=TANK, a="fixture taunt", rel=90.0, vd="OK")
add(t=100, e="act", g=RANGED, a="shoot", rel=30.0, vd="IMPOSSIBLE")
add(t=120, e="veto", g=RANGED, m="fixture spacing multiplier", a="flee")
add(t=150, e="cast", s=TANK, sp=103, tgt=BOSS, ct=0)
add(t=160, e="cast", s=RANGED, sp=100, tgt=ADD, ct=2000)
add(t=170, e="move", g=RANGED, k="point", x=-21.0, y=-3.0, z=0.0, tgt=0, ok=1, r="",
    by="flee", pr="normal")
add(t=180, e="move", g=HUMAN, k="point", x=3.5, y=1.0, z=0.0, tgt=0, ok=0, r="wait",
    by="reach melee", pr="normal", hpr="normal", hms=400)
add(t=190, e="haz", sp=105, shape="circle", x=12.0, y=0.0, z=0.0, ttl=8000, rad=6.0)
add(t=200, e="aura", d=TANK, s=BOSS, sp=105, r=0, st=1, dur=8000, p=0)
add(t=210, e="abs", d=TANK, s=HEAL, sp=102, a=1200)

hurt(300, BOSS, TANK, 104, 9000)
mend(400, HEAL, TANK, 101, 6000)
hurt(500, RANGED, ADD, 100, 4000)
hurt(600, TANK, BOSS, 103, 2500)

snapshot(1000, {BOSS: TANK, ADD: RANGED}, hazard=True)

add(t=1100, e="note", g=HEAL, k="fixture.role", txt="healer")
add(t=1200, e="aura", d=TANK, s=BOSS, sp=105, r=1, st=1, dur=0, p=0)
add(t=1300, e="cast", s=HEAL, sp=101, tgt=TANK, ct=1500, tr=1)

hurt(1500, BOSS, HEAL, 104, 12000)
hurt(1700, BOSS, HEAL, 105, 11000)
hurt(1900, BOSS, HEAL, 105, 9000, lethal=True)

add(t=1900, e="death", g=HEAL, killer=BOSS, x=-18.0, y=4.0, z=0.0,
    dist={str(BOSS): 18.4, str(ADD): 32.2},
    auras=[[105, 1, -1, BOSS, 200, -1, 0]],
    rewind=[[1500, BOSS, 104, 12000], [1700, BOSS, 105, 11000], [1900, BOSS, 105, 9000]],
    blow=[BOSS, 9000],
    hplast=[30.0, 1700],
    acts=[[100, 1300, "flash heal", 80.0, "OK", 3]],
    lastmove={"x": -18.0, "y": 4.0, "z": 0.0, "by": "fixture station action", "arrived": 1})

snapshot(2000, {BOSS: TANK})
add(t=2100, e="note", g=TANK, k="fixture.phase", txt="2")
snapshot(2500, {BOSS: HUMAN})

add(t=2600, e="cov", g=TANK, r=[[1, 40, 12, 10, 8, 1, 2], [3, 40, 0]])
add(t=2600, e="cov", g=HEAL, r=[[2, 38, 20, 18, 15]])
add(t=2600, e="cov", g=RANGED, r=[[3, 40, 5, 4, 2, 0, 1, 0, 1]])
add(t=2700, e="end", out="wipe")


def main() -> int:
    out = pathlib.Path(__file__).resolve().parent / "full-v12.ndjson"
    with out.open("w", encoding="utf-8", newline="\n") as handle:
        for rec in records:
            handle.write(json.dumps(rec, separators=(",", ":")) + "\n")
    print(f"wrote {out} ({len(records)} records)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
