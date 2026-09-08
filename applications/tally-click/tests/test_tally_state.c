#include "tally_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    tc_state s;tc_init(&s,NULL);
    assert(s.page==TC_COUNT && s.data.version==2 && s.data.count==0);
    for(int i=0;i<1000;i++) {
        assert(tc_input(&s,TC_DOWN,TC_PRESS)==TC_DIRTY);
        tc_input(&s,TC_DOWN,TC_CLICK);tc_input(&s,TC_DOWN,TC_DOUBLE);tc_input(&s,TC_DOWN,TC_LONG);
    }
    assert(s.data.count==1000);
    for(int i=0;i<10001;i++)tc_input(&s,TC_UP,TC_PRESS);
    assert(s.data.count==0);
    for(int i=0;i<10001;i++)tc_input(&s,TC_DOWN,TC_PRESS);
    assert(s.data.count==9999);
    tc_input(&s,TC_OK,TC_CLICK);assert(s.page==TC_PAUSE);
    tc_input(&s,TC_DOWN,TC_PRESS);assert(s.page==TC_HISTORY_PAGE && s.data.count==9999);
    tc_input(&s,TC_UP,TC_PRESS);assert(s.selected==0);
    tc_input(&s,TC_OK,TC_LONG);assert(s.page==TC_PAUSE);
    tc_data before=s.data,next;
    assert(tc_input(&s,TC_OK,TC_LONG)==TC_ARCHIVE);
    tc_candidate(&s,&next);assert(!memcmp(&before,&s.data,sizeof(before)));
    assert(tc_valid(&next) && next.count==0 && next.records[0].count==9999);
    s.data=next;
    for(unsigned i=0;i<25;i++) { s.data.count=i;tc_candidate(&s,&next);s.data=next; }
    assert(s.data.length==10 && s.data.serial==26);
    for(unsigned i=0;i<10;i++)assert(s.data.records[i].count==24-i && s.data.records[i].reserved==0);
    tc_state reboot;tc_init(&reboot,&s.data);
    assert(reboot.page==TC_COUNT && reboot.data.length==10);
    /* Upgrade v1 in place without keeping its wall-clock payload or losing counts. */
    tc_data legacy=s.data;legacy.version=1;legacy.count=47;legacy.reserved=123456789;
    for(unsigned i=0;i<10;i++)legacy.records[i].reserved=123456789+i;
    tc_seal(&legacy);assert(tc_valid(&legacy));tc_init(&reboot,&legacy);
    assert(reboot.page==TC_COUNT && reboot.data.count==47 && reboot.data.version==2 && !reboot.data.reserved);
    for(unsigned i=0;i<10;i++)assert(reboot.data.records[i].count==24-i && !reboot.data.records[i].reserved);
    assert(tc_valid(&reboot.data));
    for(size_t i=0;i<sizeof(tc_data);i++) {
        tc_data broken=reboot.data;((unsigned char *)&broken)[i]^=1;assert(!tc_valid(&broken));
    }
    puts("Tally state: direct counting boot, exact presses, bounds, ten records, atomic reset and v1 count migration PASS");
}
