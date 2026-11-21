# Razorscale polish log (25-man normal, no hard mode)

Reader: `tools/botobs/bosses/razorscale.py <trace>` (`--phases`, `--stack`, `--fire`, `--deaths`),
written in round 1. Ranged and healers are the `ranged`/`heal` roster roles; the stack mover is
`razorscale grounded`.

## Round 1 (build 0c08a0922, DC 7495350)

**Data:** window 5, first data, 3 valid pulls, one per pool: `tr-20261121-211523-19` (uld1),
`-212125-22` (uld2), `-211624-21` (uld3); traces
`env/dist/logs/botobs/603_{19,22,21}_razorscale_{1795295777,1795296140,1795295840}.ndjson`. No
earlier Razorscale traces exist. Harness clean: DC start from the Expedition Commander's gossip, no
DC veto rows inside a pull.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795295777 | kill 9:23 | Devouring Flame 14 (9 bots, 5 of them twice after a self-rez; 9:12-9:20, final ground phase) |
| uld2 | 1795296140 | kill 9:47 | Dark Rune Guardian melee 1 (a ranged at the south-east harpoon, 6:40, air) |
| uld3 | 1795295840 | kill 9:29 | 0 |

**Failure picked: the ranged stack walks back onto Devouring Flame** (14 of 15 deaths).

- Every `razorscale grounded` walk by a ranged or healer in all three pulls went to one spot,
  (599.54, -175.04) (2907, 1569, 2886 walks). `RazorscaleGroundedAction::Execute`
  (`UldActions_Razorscale.cpp:415-427`) used `MoveInside(centre, 12)` → `MoveNear` → the ring point
  at `GetFollowAngle()`, and `GetFollowAngle` (`MovementActions.cpp:1001-1006`) reads the master's
  group: the DC driver masters every bot from outside the group, so every bot gets angle 0. With a
  human master in the raid the slots spread round the ring.
- Nothing checked that spot for fire. A patch landed on it in the final phase of uld1 (548 s: stalker
  at exactly 599.54, -175.04). `razorscale avoid devouring flames` (`ACTION_RAID + 4`) stepped each
  bot ~10.5 yd off the patch's centre; most steps left the 12 yd ring, so the next tick
  `razorscale grounded` walked them straight back; `RazorscaleMoversBlocked` exempts ranged
  (`UldDefinition_Razorscale.cpp:35-36`) and does not cover the boss node anyway. uld1's last minute:
  1277 dodge vs 1268 stack moves for ranged/heal; 1057 stack walks ended within 10 yd of a live patch.
- uld3 had the same loop (1270 walks onto a patch in its final phase, ranged 488k and healers 207k
  flame damage on the ground) and survived on heals; uld2 had no patch on the spot (0 walks).
- The dodge measures `GetDistance2d`, so it fires up to ~10 yd from a patch's centre (90% of dodges
  started inside 10.3 yd), not 7: a spot must clear that, not `DevouringFlameBlocks`' 7.

**Classification: bug.** The strategy intends the ranged to stack inside the ring and stand out of
the fire; the stack walk undoes the dodge. The single spot is the harness's (master outside the
group), but the walk-back is the strategy's and would hit the 2-3 bots whose slot a patch covers in
human play too.

**Fix** (Razorscale files only):

- `GetRazorscaleRangedStackSpot` (`UldEncounter_Razorscale.cpp`): the bot's slot on the circle by its
  place in the group, not `GetFollowAngle`, turned in 15° steps up to 90° each way off any patch the
  dodge would move it out of (`CLEAR_RADIUS` + both combat reaches + 2 yd at the spot, the dodge's
  reach along the straight walk). No clear slot: no move.
- `RazorscaleGroundedAction::Execute` walks ranged to that spot instead of `MoveInside`; same circle
  centres, radii and "inside already" test, same `MOVEMENT_COMBAT` priority.

**Not picked:** uld2's Guardian death (one bot, a harpoon crew member caught by three Guardians, air
phase).

**Next window should show** (`razorscale.py --stack`, `--fire`, `--deaths`): stack walks spread over
many spots per ground phase (roughly one per ranged or healer, not 1); "onto a live patch" at or near
0 in every phase; no Devouring Flame deaths; ground-phase flame damage on ranged and healers well
below window 5's uld1 567k/164k and uld3 488k/207k.
