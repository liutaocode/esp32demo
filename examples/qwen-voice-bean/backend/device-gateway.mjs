#!/usr/bin/env node
/* Native-device LAN ingress. The original Gateway remains loopback-only. */
import http from 'node:http';
import {timingSafeEqual} from 'node:crypto';
import {createRequire} from 'node:module';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {parseEnv} from 'node:util';
import {fileURLToPath} from 'node:url';

const AUDIO_CHUNK_BASE64=1696; // Two complete 636-byte device PCM blocks.

export function createDeviceGateway({WebSocket,WebSocketServer,upstream,accessToken,requireToken=false}) {
  const target=new URL(upstream);
  if(!['127.0.0.1','localhost','[::1]'].includes(target.hostname))throw Error('Upstream must be loopback');
  if(!accessToken||accessToken.length<24)throw Error('Configure a private upstream access token');
  const equal=value=>{const a=Buffer.from(value),b=Buffer.from(`Bearer ${accessToken}`);return a.length===b.length&&timingSafeEqual(a,b);};
  const allowed=request=>{
    const supplied=request.headers.authorization;
    return supplied?equal(supplied):!requireToken;
  };
  const server=http.createServer((request,response)=>{
    if(request.method!=='GET'||!['/','/api/health'].includes(request.url)) {response.writeHead(404);response.end();return;}
    if(!allowed(request)){response.writeHead(401);response.end('Access token required');return;}
    const probe=http.get(new URL('/api/health',target),{headers:{Authorization:`Bearer ${accessToken}`},timeout:3000},up=>{
      let body='';up.on('data',chunk=>{body+=chunk;if(body.length>65536)up.destroy();});
      up.on('end',()=>{try{const j=JSON.parse(body);response.writeHead(200,{'Content-Type':'application/json','Cache-Control':'no-store'});response.end(JSON.stringify({ok:j.ok===true,voiceConfigured:j.voiceConfigured,devicePort:3101,tokenRequired:requireToken}));}catch{response.writeHead(502);response.end('Gateway unavailable');}});
      up.on('error',()=>{if(!response.writableEnded){response.writeHead(502);response.end('Gateway unavailable');}});
    });
    probe.on('timeout',()=>probe.destroy());probe.on('error',()=>{if(!response.writableEnded){response.writeHead(502);response.end('Gateway unavailable');}});
  });
  const wss=new WebSocketServer({noServer:true,maxPayload:65536,perMessageDeflate:false});
  server.on('upgrade',(request,socket,head)=>{
    // Browsers use the original local WebUI. Reject cross-site browser origins.
    if(request.url!=='/api/realtime'||request.headers.origin||!allowed(request)) {
      socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n');return;
    }
    const url=new URL('/api/realtime',target);url.protocol='ws:';
    const remote=new WebSocket(url,{headers:{Authorization:`Bearer ${accessToken}`},maxPayload:1048576,perMessageDeflate:false,handshakeTimeout:8000});
    let client;
    socket.once('close',()=>remote.terminate());
    remote.on('error',()=>{if(client)client.close(1011,'Gateway unavailable');else socket.end('HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n');});
    remote.on('open',()=>{
      if(socket.destroyed){remote.terminate();return;}
      wss.handleUpgrade(request,socket,head,peer=>{
        client=peer;
        const forward=(destination,data,binary)=>{
          if(binary||destination.readyState!==WebSocket.OPEN||destination.bufferedAmount>262144) {
            console.warn('Device relay closed: binary='+binary+' state='+destination.readyState+' buffered='+destination.bufferedAmount);
            peer.close(1013,'Connection congested');remote.close();return;
          }
          destination.send(data,{binary:false});
        };
        peer.on('message',(data,binary)=>{
          let event;try{event=JSON.parse(data.toString());}catch{}
          if(event?.type==='response.cancel'||event?.type==='input.mute'){
            clearOutbound();audioUntil=0;
          }
          forward(remote,data,binary);
        });
        // A voice provider can generate audio faster than real-time playback.
        // Pace the downstream queue without pausing upstream reads: control-frame
        // PINGs must still be consumed and answered while a long reply plays.
        const outbound=[];let queuedBytes=0,pumpTimer=null,audioUntil=0,lastAudioReceived=0,lastAudioSent=0;
        const clearOutbound=()=>{if(pumpTimer)clearTimeout(pumpTimer);pumpTimer=null;outbound.length=0;queuedBytes=0;};
        const pump=()=>{
          pumpTimer=null;
          if(peer.readyState!==WebSocket.OPEN){clearOutbound();return;}
          while(outbound.length && peer.bufferedAmount<32768) {
            // Bound audio already sent into nginx/TCP to about half a second.
            // Otherwise an entire long answer can sit ahead of WebSocket PONGs.
            if(outbound[0].durationMs && audioUntil>performance.now()+500)break;
            const {data,durationMs,type}=outbound.shift();queuedBytes-=Buffer.byteLength(data);
            if(durationMs){
              const now=performance.now();
              if(lastAudioSent && now-lastAudioSent>200)console.warn('Audio delivery gap ms='+Math.round(now-lastAudioSent)+' queued='+queuedBytes+' socket='+peer.bufferedAmount);
              lastAudioSent=now;audioUntil=Math.max(audioUntil,now)+durationMs;
            }
            if(type==='audio.done')lastAudioSent=0;
            peer.send(data,{binary:false},error=>{if(error)peer.terminate();});
          }
          if(outbound.length || peer.bufferedAmount>16384) {
            pumpTimer=setTimeout(pump,10);
          }
        };
        const enqueue=data=>{
          const bytes=Buffer.byteLength(data);
          if(queuedBytes+bytes>8388608) {
            console.warn('Device relay queue limit exceeded');
            clearOutbound();peer.close(1013,'Queue limit exceeded');remote.close();return false;
          }
          let event;try{event=JSON.parse(data.toString());}catch{}
          const rate=Number(event?.sampleRate)||24000;
          const durationMs=event?.type==='audio.delta'&&typeof event.audio==='string'
            ? Buffer.byteLength(event.audio,'base64')*1000/(rate*2):0;
          if(durationMs){
            const now=performance.now();
            if(lastAudioReceived && now-lastAudioReceived>200)console.warn('Audio upstream gap ms='+Math.round(now-lastAudioReceived)+' queued='+queuedBytes+' socket='+peer.bufferedAmount);
            lastAudioReceived=now;
          }
          if(event?.type==='audio.done')lastAudioReceived=0;
          outbound.push({data,durationMs,type:event?.type});queuedBytes+=bytes;return true;
        };
        remote.on('message',(data,binary)=>{
          if(binary){peer.close(1003,'Text messages required');remote.close();return;}
          if(peer.readyState!==WebSocket.OPEN)return;
          let event;try{event=JSON.parse(data.toString());}catch{}
          if(event?.type==='audio.delta' && typeof event.audio==='string' && event.audio.length>AUDIO_CHUNK_BASE64) {
            for(let offset=0;offset<event.audio.length;offset+=AUDIO_CHUNK_BASE64) {
              const chunk={...event,audio:event.audio.slice(offset,offset+AUDIO_CHUNK_BASE64)};
              if(event.event_id)chunk.event_id=`${event.event_id}_${offset/AUDIO_CHUNK_BASE64}`;
              if(!enqueue(JSON.stringify(chunk)))return;
            }
          } else if(!enqueue(data))return;
          if(!pumpTimer)pump();
        });
        peer.once('close',clearOutbound);
        peer.on('error',()=>remote.terminate());peer.on('close',code=>{console.info('Device socket closed: code='+code);remote.close();});
        remote.on('close',code=>{console.info('Upstream socket closed: code='+code);peer.close(code===1000?1000:1011,'Gateway disconnected');});
      });
    });
  });
  const closeServer=server.close.bind(server);
  server.close=(...args)=>{for(const client of wss.clients)client.terminate();return closeServer(...args);};
  server.on('close',()=>wss.close());
  return server;
}

if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const option=name=>{const i=process.argv.indexOf(name);if(i<0||!process.argv[i+1])throw Error(`Missing ${name}`);return process.argv[i+1];};
  const runtime=resolve(option('--runtime')),config=resolve(option('--config'));
  const require=createRequire(resolve(runtime,'package.json'));
  const {WebSocket,WebSocketServer}=require('ws');
  const env=parseEnv(readFileSync(resolve(config,'config.env'),'utf8'));
  const port=Number(env.PORT||3102);
  const server=createDeviceGateway({WebSocket,WebSocketServer,upstream:`http://127.0.0.1:${port}`,
    accessToken:env.QWEN_AUDIO_GATEWAY_ACCESS_TOKEN,requireToken:env.BEAN_DEVICE_REQUIRE_TOKEN==='1'});
  server.listen(3101,'0.0.0.0',()=>console.log('Device gateway ready on port 3101; token required: '+(env.BEAN_DEVICE_REQUIRE_TOKEN==='1')));
  for(const signal of ['SIGTERM','SIGINT'])process.on(signal,()=>server.close(()=>process.exit(0)));
}
