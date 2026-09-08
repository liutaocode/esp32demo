#include "tally_sound.h"
#include "tally_pcm_data.h"
unsigned tc_sound_length(tc_feedback s)
{ return s>=TC_FX_NONE && s<=TC_FX_ERROR ? tc_pcm_lengths[s]:0; }
int16_t tc_sound_sample(tc_feedback s,unsigned i)
{ return i<tc_sound_length(s) ? tc_pcm_data[s][i]:0; }
bool tc_mixer_busy(const tc_mixer *m)
{ return m->position<tc_sound_length(m->current) || m->fade!=0; }
void tc_mixer_request(tc_mixer *m,tc_feedback sound)
{
    if(sound<=TC_FX_NONE || sound>TC_FX_ERROR)return;
    m->previous=m->current; m->previous_position=m->position;
    m->fade=tc_mixer_busy(m) ? TC_CROSSFADE_FRAMES:0;
    m->current=sound; m->position=0;
}
void tc_mixer_render(tc_mixer *m,int16_t *out,unsigned count)
{
    for(unsigned i=0;i<count;i++) {
        int32_t next=tc_sound_sample(m->current,m->position);
        if(m->position<tc_sound_length(m->current))++m->position;
        if(m->fade) {
            int32_t old=tc_sound_sample(m->previous,m->previous_position++);
            next=(old*(int32_t)m->fade+next*(int32_t)(TC_CROSSFADE_FRAMES-m->fade))/(int32_t)TC_CROSSFADE_FRAMES;
            --m->fade;
        }
        out[i]=(int16_t)next;
    }
}
