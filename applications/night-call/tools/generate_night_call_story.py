#!/usr/bin/env python3
"""Generate immutable story tables and the exact spoken subtitle list."""
from pathlib import Path
import json, argparse
R=Path(__file__).resolve().parents[1]
A=R/'main/apps/night_call'
s=json.loads((A/'story.json').read_text())
ids={n['id']:i for i,n in enumerate(s['nodes'])}
def reflow(v):
    v=v.replace('\n',''); lines=[]
    while v:
        cut=min(12,len(v))
        if cut<len(v) and v[cut] in '，。？！：；、': cut-=1
        lines.append(v[:cut]);v=v[cut:]
    return '\n'.join(lines)
def q(v): return json.dumps(v,ensure_ascii=False)
def target(t):
    if t=='resolve': return 1000
    if t=='home': return 1001
    if t.startswith('e'): return -int(t[1:])-1
    return ids[t]
lines=['#include "night_call_state.h"','const nc_node_t nc_nodes[] = {']
for n in s['nodes']:
    choices=', '.join('{'+q(c['text'])+', '+str(target(c['target']))+'}' for c in n['choices'])
    lines.append('    {'+q(n['title'])+', '+q(reflow(n['body']))+', {'+choices+'}, '+str(n['clue'])+', '+str('abc'.index(n['id'][0]))+'},')
lines+=['};',f'const unsigned nc_node_count = {len(ids)};', 'const unsigned nc_starts[NC_CHAPTERS] = {'+', '.join(str(ids[x]) for x in s['starts'])+'};',f'const unsigned nc_missing_node = {ids["c6"]};', 'const char *const nc_chapters[NC_CHAPTERS] = {'+', '.join(q(x) for x in s['chapters'])+'};','const nc_ending_t nc_endings[NC_ENDINGS] = {']
lines+=['    {'+', '.join(q(reflow(e[k]) if k=='body' else e[k]) for k in ('title','body','hint'))+'},' for e in s['endings']]
lines+=['};','const nc_clue_t nc_clues[NC_CLUES] = {']
lines+=['    {'+q(c['title'])+', '+q(reflow(c['body']))+'},' for c in s['clues']]
lines+=['};','']
voices=[n['body'].replace('\n','') for n in s['nodes']]+[e['body'].replace('\n','') for e in s['endings']]
outputs={A/'night_call_story.c':'\n'.join(lines),A/'voice_text.json':json.dumps(voices,ensure_ascii=False,indent=2)+'\n'}
p=argparse.ArgumentParser(); p.add_argument('--check',action='store_true'); args=p.parse_args()
for f,c in outputs.items():
    if args.check: assert f.read_text()==c,f'Stale: {f}'
    else: f.write_text(c)
print(f'Story: {len(ids)} scenes, {len(voices)} Mandarin clips, 7 endings')
