#!/usr/bin/env python3
"""Check the reviewed publication allowlist against source and build entrypoints."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    catalog = json.loads((ROOT / 'tools/published_apps.json').read_text())
    apps = catalog['applications']
    ids = [app['community_id'] for app in apps]
    if len(set(ids)) != len(ids):
        raise SystemExit('Duplicate community project IDs')
    selectors = set()
    standalone = set()
    for app in apps:
        if app['project_status'] != 'published':
            raise SystemExit(f"Unpublished project in allowlist: {app['community_id']}")
        source = ROOT / app['source_directory']
        if not source.resolve().is_relative_to(ROOT.resolve()):
            raise SystemExit('Source path escapes the repository')
        if not (source / 'main/CMakeLists.txt').is_file():
            raise SystemExit(f'Missing build entrypoint: {source.relative_to(ROOT)}')
        if app['build_selector'] is not None:
            selectors.add(app['build_selector'])
        else:
            standalone.add(source.name)
    cmake = (ROOT / 'main/CMakeLists.txt').read_text()
    configured = set(re.findall(r'selected_app STREQUAL "([a-z0-9_]+)"', cmake))
    if configured != selectors:
        raise SystemExit(f'Selector mismatch: {sorted(configured ^ selectors)}')
    directories = {p.name for p in (ROOT / 'main/apps').iterdir() if p.is_dir()}
    if directories != selectors - {'minecraft_guide'}:
        raise SystemExit('Application source directories do not match the allowlist')
    if {p.name for p in (ROOT / 'applications').iterdir() if p.is_dir()} != standalone:
        raise SystemExit('Standalone project directories do not match the allowlist')
    for name in re.findall(r'"([^"\n]+\.c)"', cmake):
        if not (ROOT / 'main' / name).is_file():
            raise SystemExit(f'Missing compiled source: {name}')
    print(f'Publication allowlist: PASS ({len(selectors)} selectors, {len(standalone)} standalone projects)')


if __name__ == '__main__':
    main()
