#!/usr/bin/env python3
"""Start, stop, and inspect an isolated Qwen Audio Agent without printing secrets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
import urllib.request

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['start','stop','restart','status'])
parser.add_argument('--runtime',type=Path,default=Path.home()/'.local/share/mouthy-bean-online/qwen-audio-agent')
parser.add_argument('--config',type=Path,default=Path.home()/'.config/mouthy-bean-online')
parser.add_argument('--node',default=os.environ.get('ONLINE_NODE') or shutil.which('node'))
args=parser.parse_args()
args.runtime=args.runtime.resolve();args.config=args.config.resolve()
private_node=args.runtime.parent/'runtime/node_modules/node/bin/node'
if not os.environ.get('ONLINE_NODE') and '--node' not in sys.argv and private_node.exists():args.node=str(private_node)
args.config.mkdir(parents=True,exist_ok=True,mode=0o700)
pidfile=args.config/'gateway.pid'
entry=args.runtime/'server/src/index.mjs'
relay=Path(__file__).resolve().with_name('device-gateway.mjs')
relay_pidfile=args.config/'device-gateway.pid'

def running(path=pidfile,expected=entry):
    try:
        pid=int(path.read_text())
        command=subprocess.check_output(['ps','-p',str(pid),'-o','command='],text=True).strip()
        return pid if str(expected) in command else None
    except (OSError,ValueError,subprocess.CalledProcessError):
        return None

def stop():
    for path,expected in [(relay_pidfile,relay),(pidfile,entry)]:
        pid=running(path,expected)
        if pid:
            os.kill(pid,signal.SIGTERM)
            for _ in range(50):
                if not running(path,expected):break
                time.sleep(.1)
            if running(path,expected):raise SystemExit('Service is still stopping; retry status in a moment.')
        path.unlink(missing_ok=True)

def health():
    try:
        with urllib.request.urlopen('http://127.0.0.1:3102/api/health',timeout=3) as response:
            info=json.load(response)
        # Never dump arbitrary health payloads or configuration.
        return {'http':'PASS','voice_configured':info.get('voiceConfigured'),
                'backend':info.get('backend',{}).get('protocol') if isinstance(info.get('backend'),dict) else info.get('backend')}
    except Exception:
        return {'http':'UNAVAILABLE'}

if args.action in ('stop','restart'):stop()
if args.action in ('start','restart'):
    if running():print('Gateway is already running.')
    else:
        if not args.node or not entry.exists():raise SystemExit('Install Node and the pinned Qwen Audio Agent source first.')
        env=os.environ.copy();env['QWAUDIO_CONFIG_DIR']=str(args.config)
        env['PATH']=str(Path(args.node).parent)+os.pathsep+env.get('PATH','')
        check="import {loadRuntimeEnvironment} from './shared/runtime-environment.mjs';import {gatewaySetupStatus} from './shared/gateway/setup.mjs';loadRuntimeEnvironment({root:process.cwd()});console.log(JSON.stringify({ready:gatewaySetupStatus().ready}));"
        result=subprocess.run([args.node,'--input-type=module','-e',check],cwd=args.runtime,env=env,capture_output=True,text=True)
        if result.returncode or not json.loads(result.stdout.strip()).get('ready'):
            raise SystemExit('Not started: fill DASHSCOPE_API_KEY in '+str(args.config/'config.env')+' and retry. Do not paste the key into chat.')
        logfile=args.config/'service.log'
        fd=os.open(logfile,os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
        with os.fdopen(fd,'ab',buffering=0) as log:
            process=subprocess.Popen([args.node,str(entry)],cwd=args.config/'workspace',env=env,
                stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
        pidfile.write_text(str(process.pid));pidfile.chmod(0o600)
        time.sleep(2)
        if process.poll() is not None:raise SystemExit('Gateway exited. Inspect the private service.log locally; do not publish it.')
        print('Gateway started, PID '+str(process.pid))
if args.action in ('start','restart') and running() and not running(relay_pidfile,relay):
    if health().get('http')!='PASS':raise SystemExit('Gateway is not ready; retry start after checking private logs.')
    fd=os.open(args.config/'device-gateway.log',os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
    with os.fdopen(fd,'ab',buffering=0) as log:
        process=subprocess.Popen([args.node,str(relay),'--runtime',str(args.runtime),'--config',str(args.config)],
            stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
    relay_pidfile.write_text(str(process.pid));relay_pidfile.chmod(0o600)
    time.sleep(.5)
    if process.poll() is not None:raise SystemExit('Device gateway failed; inspect its private log.')
    print('Device gateway started on port 3101.')
if args.action!='stop':print(json.dumps({'process':'RUNNING' if running() else 'STOPPED','device_gateway':'RUNNING' if running(relay_pidfile,relay) else 'STOPPED',**health()},ensure_ascii=False))
else:print('Gateway and device gateway stopped.')
