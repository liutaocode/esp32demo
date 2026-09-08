/* Offline voice-bank converter. Requires opencore-amrwb and Speex 1.2.1.
 * Input is the checksum-pinned Xiaole bank; output keeps its pronunciation tables.
 * Speex quality is selected at build time, never encoded on the microcontroller. */
#include <speex/speex.h>
#include "tts_tempo.h"
#include <opencore-amrwb/dec_if.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void w16(FILE *f,unsigned n){fputc(n&255,f);fputc(n>>8,f);}
static void w32(FILE *f,unsigned n){w16(f,n&65535);w16(f,n>>16);}
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"usage: %s original.dat compact.dat quality(0..3)\n",argv[0]);return 2;}
    int quality=atoi(argv[3]);assert(quality>=0&&quality<=3);
    FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    unsigned char *in=malloc(size);assert(in&&fread(in,1,size,f)==(size_t)size);fclose(f);
    unsigned meta=40+u32(in+36),count=u32(in+20)/4-1;assert(meta<(unsigned)size&&count==1749);
    unsigned char *header=malloc(meta);assert(header);memcpy(header,in,meta);
    memset(header,0,20);snprintf((char*)header,16,"xiaole_spx_q%d",quality);put32(header+16,0x31585053);
    f=fopen(argv[2],"wb+");assert(f);assert(fwrite(header,1,meta,f)==meta);
    const int amr_bytes[16]={18,24,33,37,41,47,51,59,61,6,0,0,0,0,1,1};
    unsigned total_frames=0,packet_bytes=0;
    for(unsigned syll=0;syll<count;syll++) {
        const unsigned char *p=in+meta+u32(in+40+syll*4),*end=in+meta+u32(in+44+syll*4);
        assert(p<end&&end<=in+size&&!memcmp(p,"#!AMR-WB\n",9));p+=9;
        short pcm16[320*128];unsigned samples16=0;void *amr=D_IF_init();assert(amr);
        while(p<end){unsigned mode=(*p>>3)&15;int n=amr_bytes[mode];assert(n>0&&p+n<=end&&samples16+320<=320*128);
            D_IF_decode(amr,p,pcm16+samples16,0);samples16+=320;p+=n;}
        D_IF_exit(amr);
        /* Restore the earlier demo pacing offline while retaining the full 16 kHz band.
           Default on-device playback can then decode frames without tempo preparation. */
        short pcm_fast[320*200] = {0};
        unsigned samples = (unsigned)tts_tempo_process(pcm16, samples16, pcm_fast, 320*200, 5);
        assert(samples > 0);
        void *enc=speex_encoder_init(&speex_wb_mode);assert(enc);int rate=16000,complexity=8,lookahead=0,frame=0;
        assert(speex_encoder_ctl(enc,SPEEX_SET_QUALITY,&quality)==0);
        speex_encoder_ctl(enc,SPEEX_SET_SAMPLING_RATE,&rate);speex_encoder_ctl(enc,SPEEX_SET_COMPLEXITY,&complexity);
        speex_encoder_ctl(enc,SPEEX_GET_LOOKAHEAD,&lookahead);speex_encoder_ctl(enc,SPEEX_GET_FRAME_SIZE,&frame);assert(frame==320);
        unsigned frames=(samples+lookahead+319)/320;
        unsigned pos=(unsigned)ftell(f)-meta;put32(header+40+syll*4,pos);
        w32(f,samples);w16(f,lookahead);w16(f,frames);
        SpeexBits bits;speex_bits_init(&bits);
        for(unsigned i=0;i<frames;i++){speex_bits_reset(&bits);speex_encode_int(enc,pcm_fast+320*i,&bits);char packet[128];
            int bytes=speex_bits_write(&bits,packet,sizeof(packet));assert(bytes>0&&bytes<=128);
            if(packet_bytes)assert((unsigned)bytes==packet_bytes);packet_bytes=bytes;
            assert(fwrite(packet,1,bytes,f)==(unsigned)bytes);total_frames++;}
        speex_bits_destroy(&bits);speex_encoder_destroy(enc);
    }
    unsigned audio_bytes=(unsigned)ftell(f)-meta;put32(header+40+count*4,audio_bytes);
    /* The reserved header word stores version, quality, and fixed packet bytes. */
    put32(header+16,0x53020000u|((unsigned)quality<<8)|packet_bytes);
    rewind(f);assert(fwrite(header,1,meta,f)==meta);fclose(f);
    printf("quality=%d syllables=%u frame_bytes=%u frames=%u metadata=%u audio=%u total=%u\n",quality,count,packet_bytes,total_frames,meta,audio_bytes,meta+audio_bytes);
    free(header);free(in);return 0;
}
