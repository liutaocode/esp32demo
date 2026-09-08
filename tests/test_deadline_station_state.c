#include "deadline_station_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    assert(ds_date_utc((ds_date_t){2026,9,5,8,0})==1788566400LL);
    assert(!ds_date_valid((ds_date_t){2027,2,29,0,0}));
    assert(ds_date_valid((ds_date_t){2028,2,29,0,0}));
    assert(ds_month_days(2100,2)==28 && ds_month_days(2000,2)==29);
    for(int y=2026;y<=2099;y++) for(int m=1;m<=12;m++) for(int day=1;day<=ds_month_days(y,m);day++) {
        ds_date_t d={y,m,day,23,59},got=ds_beijing(ds_date_utc(d));
        assert(d.year==got.year && d.month==got.month && d.day==got.day && d.hour==got.hour && d.minute==got.minute);
    }
    ds_date_t d={2028,2,29,23,59};ds_date_move(&d,0,1);assert(d.year==2029 && d.day==28);
    ds_date_move(&d,4,1);assert(d.minute==0);
    ds_date_move(&d,3,1);assert(d.hour==0);
    const ds_entry_t *cvpr=&ds_catalog[0];assert(cvpr->count==3);
    d=ds_beijing(cvpr->nodes[1].utc);assert(d.year==2026 && d.month==11 && d.day==17 && d.hour==19 && d.minute==59);
    assert(cvpr->nodes[1].utc%60==59);
    assert(ds_next_node(cvpr,cvpr->nodes[0].utc-1)==0);
    assert(ds_next_node(cvpr,cvpr->nodes[0].utc)==1);
    assert(ds_next_node(cvpr,cvpr->nodes[2].utc)==-1);
    assert(ds_urgency(100,100)==0 && ds_urgency(101,100)==1);
    assert(ds_urgency(86400+100,100)==1 && ds_urgency(86401+100,100)==2);
    ds_progress_t p={.seed={2026,9,5,8,0}};unsigned order[DS_MAX_ENTRIES];
    unsigned n=ds_list(order,0,2027,&p,1788566400LL);assert(n>10);
    assert(strcmp(ds_catalog[order[0]].track,"产业论文")==0);
    assert(ds_list(order,5,2027,&p,1788566400LL)==0);
    p.flags[0]=31;p.sessions=65535;
    assert(ds_list(order,5,2027,&p,1788566400LL)==1 && order[0]==0);
    assert(ds_checks(31)==4);
    n=ds_list(order,1,2028,&p,1788566400LL);assert(n>0);
    for(unsigned i=0;i<n;i++)assert(ds_catalog[order[i]].count==0 && ds_catalog[order[i]].field==1);
    uint8_t bytes[DS_SAVE_SIZE];ds_encode(&p,bytes);ds_progress_t restored={0};
    assert(ds_decode(&restored,bytes) && restored.flags[0]==31 && restored.sessions==65535);
    /* IDs preserve progress when catalog records are reordered. */
    uint8_t record[3];memcpy(record,bytes+12,3);memcpy(bytes+12,bytes+15,3);memcpy(bytes+15,record,3);
    assert(ds_decode(&restored,bytes) && restored.flags[0]==31);
    bytes[2]=99;assert(!ds_decode(&restored,bytes));
    ds_encode(&p,bytes);bytes[7]=32;assert(!ds_decode(&restored,bytes));
    puts("Deadline Station state: dates, 27028 day round trips, deadlines, filtering, persistence PASS");
}
