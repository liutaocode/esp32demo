#!/usr/bin/env python3
"""Prove every Pocket Arcade push-the-box level can actually be finished.

A level that cannot be solved would strand the player with no way forward, so
the level table is searched here rather than trusted.
"""

from __future__ import annotations

from heapq import heappop, heappush
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main/apps/pocket_arcade/pa_g_sokoban.c"
ROWS = 8
COLUMNS = 9
DIRECTIONS = ((1, 0), (0, 1), (-1, 0), (0, -1))


def load_levels() -> list[list[str]]:
    text = SOURCE.read_text(encoding="utf-8")
    start = text.index("PA_SOK_LEVEL")
    body = text[start:text.index("};", start)]
    lines = re.findall(r'"([^"]*)"', body)
    if len(lines) % ROWS:
        raise SystemExit(f"level table holds {len(lines)} rows, not a multiple of {ROWS}")
    return [lines[i:i + ROWS] for i in range(0, len(lines), ROWS)]


def parse(level: list[str]):
    walls, goals, boxes, player = set(), set(), set(), None
    for y, row in enumerate(level):
        if len(row) != COLUMNS:
            raise SystemExit(f"row {y} is {len(row)} wide, expected {COLUMNS}")
        for x, cell in enumerate(row):
            if cell == "#":
                walls.add((x, y))
            if cell in ".*+":
                goals.add((x, y))
            if cell in "$*":
                boxes.add((x, y))
            if cell in "@+":
                player = (x, y)
    if player is None:
        raise SystemExit("level has no player")
    if len(boxes) != len(goals):
        raise SystemExit(f"level has {len(boxes)} boxes and {len(goals)} goals")
    return walls, goals, frozenset(boxes), player


def build(level: list[str]):
    """Flatten the level into bitmasks so the search can stay in integers."""
    walls, goals, boxes, player = parse(level)
    size = ROWS * COLUMNS
    index = lambda cell: cell[1] * COLUMNS + cell[0]
    wall_mask = 0
    for cell in walls:
        wall_mask |= 1 << index(cell)
    neighbours = []
    for position in range(size):
        x, y = position % COLUMNS, position // COLUMNS
        row = []
        for dx, dy in DIRECTIONS:
            nx, ny = x + dx, y + dy
            inside = 0 <= nx < COLUMNS and 0 <= ny < ROWS
            row.append(ny * COLUMNS + nx if inside else -1)
        neighbours.append(row)
    goal_mask = 0
    for cell in goals:
        goal_mask |= 1 << index(cell)
    box_mask = 0
    for cell in boxes:
        box_mask |= 1 << index(cell)
    return wall_mask, goal_mask, box_mask, index(player), neighbours, size


def zone_of(wall_mask, box_mask, start, neighbours):
    """Bitmask of every square the player can walk to without pushing."""
    seen = 1 << start
    stack = [start]
    blocked = wall_mask | box_mask
    while stack:
        position = stack.pop()
        for step in neighbours[position]:
            if step < 0:
                continue
            bit = 1 << step
            if seen & bit or blocked & bit:
                continue
            seen |= bit
            stack.append(step)
    return seen


def corner_mask(wall_mask, goal_mask, neighbours, size):
    """Squares where a box off a goal would be wedged for good."""
    mask = 0
    for position in range(size):
        bit = 1 << position
        if wall_mask & bit or goal_mask & bit:
            continue
        right, down, left, up = neighbours[position]
        vertical = (up < 0 or wall_mask >> up & 1) or (down < 0 or wall_mask >> down & 1)
        horizontal = (left < 0 or wall_mask >> left & 1) or (right < 0 or wall_mask >> right & 1)
        if vertical and horizontal:
            mask |= bit
    return mask


def distance_table(goal_mask, size):
    """Manhattan distance from every square to its nearest goal."""
    goals = [g for g in range(size) if goal_mask >> g & 1]
    table = []
    for position in range(size):
        x, y = position % COLUMNS, position // COLUMNS
        table.append(min(abs(x - g % COLUMNS) + abs(y - g // COLUMNS) for g in goals))
    return table


def solve(level: list[str]) -> int | None:
    """Search for any finish, guided by how far the boxes still are from goals."""
    wall_mask, goal_mask, box_mask, player, neighbours, size = build(level)
    dead = corner_mask(wall_mask, goal_mask, neighbours, size)
    if box_mask & dead:
        return None
    distance = distance_table(goal_mask, size)

    def spread(boxes):
        total, remaining = 0, boxes
        while remaining:
            bit = remaining & -remaining
            remaining ^= bit
            total += distance[bit.bit_length() - 1]
        return total

    zone = zone_of(wall_mask, box_mask, player, neighbours)
    seen = {(box_mask, zone & -zone)}
    queue = [(spread(box_mask), 0, box_mask, zone)]
    while queue:
        _, pushes, boxes, walkable = heappop(queue)
        if boxes & goal_mask == boxes:
            return pushes
        remaining = boxes
        while remaining:
            bit = remaining & -remaining
            remaining ^= bit
            position = bit.bit_length() - 1
            for direction in range(4):
                target = neighbours[position][direction]
                stand = neighbours[position][(direction + 2) % 4]
                if target < 0 or stand < 0:
                    continue
                if not walkable >> stand & 1:
                    continue
                target_bit = 1 << target
                if (wall_mask | boxes) & target_bit or dead & target_bit:
                    continue
                moved = boxes ^ bit | target_bit
                next_zone = zone_of(wall_mask, moved, position, neighbours)
                key = (moved, next_zone & -next_zone)
                if key in seen:
                    continue
                seen.add(key)
                heappush(queue, (pushes + 1 + spread(moved), pushes + 1, moved, next_zone))
    return None


def main() -> int:
    levels = load_levels()
    if not levels:
        raise SystemExit("no levels found")
    failures = []
    for index, level in enumerate(levels, start=1):
        pushes = solve(level)
        if pushes is None:
            failures.append(index)
            print(f"level {index}: NO SOLUTION", file=sys.stderr)
        else:
            print(f"level {index}: solvable in {pushes} pushes")
    if failures:
        raise SystemExit(f"unsolvable levels: {failures}")
    print(f"Pocket Arcade levels: {len(levels)} solvable")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
