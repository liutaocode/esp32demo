#!/usr/bin/env python3
"""Design-time generator for Pocket Arcade push-the-box levels.

Levels are built backwards: boxes start on their goals and are pulled away, so
the finished layout is reachable by construction. Every candidate is then run
through a bounded solver, and only the ones that really finish are printed, with
the number of pushes an optimal player needs. Levels chosen this way were pasted
into `main/apps/pocket_arcade/pa_g_sokoban.c`, where the gate's own solver in
`tools/test_pocket_arcade_levels.py` checks them again on every run.

    python3 tools/generate_pocket_arcade_levels.py --seed 11 --seconds 90

This is not part of the build or the gate; it is the tool that produced the
level table, kept so the table can be regrown or replaced.
"""
import argparse
import json
import random
import time
from pathlib import Path
from collections import deque
from heapq import heappush, heappop

W, H = 9, 8
DIRS = ((1, 0), (0, 1), (-1, 0), (0, -1))
SIZE = W * H
NEIGH = []
for pos in range(SIZE):
    x, y = pos % W, pos // W
    NEIGH.append([(y + dy) * W + (x + dx) if 0 <= x + dx < W and 0 <= y + dy < H else -1
                  for dx, dy in DIRS])

def zone_of(wall, boxes, start):
    seen, stack, blocked = 1 << start, [start], wall | boxes
    while stack:
        p = stack.pop()
        for n in NEIGH[p]:
            if n < 0: continue
            bit = 1 << n
            if seen & bit or blocked & bit: continue
            seen |= bit; stack.append(n)
    return seen

def corners(wall, goal):
    mask = 0
    for p in range(SIZE):
        bit = 1 << p
        if wall & bit or goal & bit: continue
        r, d, l, u = NEIGH[p]
        vert = (u < 0 or wall >> u & 1) or (d < 0 or wall >> d & 1)
        horiz = (l < 0 or wall >> l & 1) or (r < 0 or wall >> r & 1)
        if vert and horiz: mask |= bit
    return mask

def solve(wall, goal, boxes, player, cap=40000):
    dead = corners(wall, goal)
    if boxes & dead: return None
    goals = [g for g in range(SIZE) if goal >> g & 1]
    dist = [min(abs(p % W - g % W) + abs(p // W - g // W) for g in goals) for p in range(SIZE)]
    def spread(bs):
        t, r = 0, bs
        while r:
            b = r & -r; r ^= b; t += dist[b.bit_length() - 1]
        return t
    zone = zone_of(wall, boxes, player)
    seen = {(boxes, zone & -zone)}
    heap = [(spread(boxes), 0, boxes, zone)]
    expanded = 0
    while heap:
        expanded += 1
        if expanded > cap: return None
        _, pushes, bs, walk = heappop(heap)
        if bs & goal == bs: return pushes
        r = bs
        while r:
            bit = r & -r; r ^= bit
            pos = bit.bit_length() - 1
            for d in range(4):
                target, stand = NEIGH[pos][d], NEIGH[pos][(d + 2) % 4]
                if target < 0 or stand < 0 or not walk >> stand & 1: continue
                tb = 1 << target
                if (wall | bs) & tb or dead & tb: continue
                moved = bs ^ bit | tb
                nz = zone_of(wall, moved, pos)
                key = (moved, nz & -nz)
                if key in seen: continue
                seen.add(key)
                heappush(heap, (pushes + 1 + spread(moved), pushes + 1, moved, nz))
    return None

def build(rng):
    wall = 0
    for p in range(SIZE):
        x, y = p % W, p // W
        if x in (0, W - 1) or y in (0, H - 1): wall |= 1 << p
    for _ in range(rng.randint(3, 8)):
        wall |= 1 << (rng.randint(2, H - 3) * W + rng.randint(2, W - 3))
    free = [p for p in range(SIZE) if not wall >> p & 1]
    n = rng.choice([3, 3, 4, 4])
    if len(free) < n + 8: return None
    picks = rng.sample(free, n + 1)
    goal = 0
    for p in picks[:n]: goal |= 1 << p
    boxes, player = goal, picks[n]
    for _ in range(rng.randint(10, 30)):
        zone = zone_of(wall, boxes, player)
        opts = []
        r = boxes
        while r:
            bit = r & -r; r ^= bit
            pos = bit.bit_length() - 1
            for d in range(4):
                stand, back = NEIGH[pos][d], None
                if stand < 0: continue
                bx, by = stand % W, stand // W
                dx, dy = DIRS[d]
                nx, ny = bx + dx, by + dy
                if not (0 <= nx < W and 0 <= ny < H): continue
                back = ny * W + nx
                if (wall | boxes) >> stand & 1 or not zone >> stand & 1: continue
                if (wall | boxes) >> back & 1: continue
                opts.append((bit, stand, back))
        if not opts: break
        bit, stand, back = rng.choice(opts)
        boxes = boxes ^ bit | (1 << stand)
        player = back
    if boxes == goal: return None
    return wall, goal, boxes, player

def render(wall, goal, boxes, player):
    rows = []
    for y in range(H):
        row = ""
        for x in range(W):
            p = y * W + x
            c = "#" if wall >> p & 1 else " "
            if goal >> p & 1: c = "."
            if boxes >> p & 1: c = "*" if goal >> p & 1 else "$"
            if p == player: c = "+" if goal >> p & 1 else "@"
            row += c
        rows.append(row)
    return rows

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=11)
    parser.add_argument("--seconds", type=float, default=60.0)
    parser.add_argument("--min-pushes", type=int, default=8)
    parser.add_argument("--out", type=Path, default=None,
                        help="write every accepted level to this JSON file")
    args = parser.parse_args()

    rng = random.Random(args.seed)
    started = time.time()
    found: dict[int, list[list[str]]] = {}
    tries = 0
    while time.time() - started < args.seconds:
        tries += 1
        made = build(rng)
        if not made:
            continue
        pushes = solve(*made)
        if pushes is None or pushes < args.min_pushes:
            continue
        found.setdefault(pushes, []).append(render(*made))
    if args.out:
        args.out.write_text(json.dumps({str(k): v for k, v in found.items()}, indent=1))
    for pushes in sorted(found):
        print(f"{pushes} pushes: {len(found[pushes])} levels")
        for row in found[pushes][0]:
            print(f"  {row}")
    print(f"{tries} candidates tried")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
