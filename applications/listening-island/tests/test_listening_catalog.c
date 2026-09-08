#include "listening_catalog.h"
#include "minecraft_adpcm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 uint8_t *banks[2];for(unsigned b=0;b<2;b++){
  FILE *f=fopen(b?"assets/music/listening/listening_b.bin":"assets/music/listening/listening_a.bin","rb");assert(f);
  banks[b]=malloc(li_bank_sizes[b]);assert(banks[b]);assert(fread(banks[b],1,li_bank_sizes[b],f)==li_bank_sizes[b]);assert(fgetc(f)==EOF);fclose(f);
 }
 uint32_t offsets[2]={0};unsigned totals=0;
 for(unsigned i=0;i<LI_COUNT;i++){
  const li_clip_t *c=&li_clips[i];assert(li_clip_valid(c,li_bank_sizes[c->bank]));assert(c->offset==offsets[c->bank]);offsets[c->bank]+=c->bytes;
  for(unsigned j=i+1;j<(i/LI_PER_TOPIC+1)*LI_PER_TOPIC;j++)assert(strcmp(li_meanings[i],li_meanings[j])!=0);
  minecraft_adpcm_state_t decoder;minecraft_adpcm_init(&decoder,c->predictor,c->step);int peak=0;
  for(unsigned n=0;n<c->samples-1;n++){uint8_t b=banks[c->bank][c->offset+n/2];int x=minecraft_adpcm_decode(&decoder,(n&1)?b>>4:b&15);if(abs(x)>peak)peak=abs(x);}
  assert(peak>1000);totals+=c->samples;
 }
 for(unsigned b=0;b<2;b++){assert(offsets[b]==li_bank_sizes[b]);free(banks[b]);}
 li_clip_t bad=li_clips[0];bad.offset=UINT32_MAX;assert(!li_clip_valid(&bad,li_bank_sizes[0]));bad=li_clips[0];bad.bank=2;assert(!li_clip_valid(&bad,li_bank_sizes[0]));bad=li_clips[0];bad.samples=0;assert(!li_clip_valid(&bad,li_bank_sizes[0]));bad=li_clips[0];bad.step=99;assert(!li_clip_valid(&bad,li_bank_sizes[0]));
 printf("Audio catalog: PASS (%u clips, %u samples, unique meanings within each topic, both banks decoded)\n",LI_COUNT,totals);
}
