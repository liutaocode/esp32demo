#include "deadline_station_state.h"
#include <string.h>

const char *const ds_fields[6] = {"全部方向", "计算机视觉", "自然语言", "机器学习", "人工智能", "我的收藏"};
int ds_month_days(int y, int m)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 0;
    return days[m-1] + (m == 2 && y%4 == 0 && (y%100 != 0 || y%400 == 0));
}
bool ds_date_valid(ds_date_t d)
{
    return d.year >= 2026 && d.year <= 2099 && d.month >= 1 && d.month <= 12 &&
        d.day >= 1 && d.day <= ds_month_days(d.year,d.month) &&
        d.hour >= 0 && d.hour < 24 && d.minute >= 0 && d.minute < 60;
}
int64_t ds_date_utc(ds_date_t d)
{
    if (!ds_date_valid(d)) return -1;
    int64_t days = 0;
    for (int y=1970;y<d.year;y++) days += 365 + (y%4==0 && (y%100!=0 || y%400==0));
    for (int m=1;m<d.month;m++) days += ds_month_days(d.year,m);
    return (days+d.day-1)*86400 + d.hour*3600 + d.minute*60 - 8*3600;
}
ds_date_t ds_beijing(int64_t utc)
{
    int64_t local = utc + 8*3600;
    if (local < 0) local = 0;
    ds_date_t d = {.year=1970,.month=1,.day=1,.hour=(int)(local%86400/3600),.minute=(int)(local%3600/60)};
    int64_t days = local/86400;
    for (;;) {
        int n=365 + (d.year%4==0 && (d.year%100!=0 || d.year%400==0));
        if (days<n) break;
        days-=n; d.year++;
    }
    while (days>=ds_month_days(d.year,d.month)) days-=ds_month_days(d.year,d.month++);
    d.day+=(int)days; return d;
}
static int wrap(int value, int lo, int hi, int delta)
{
    int n=hi-lo+1;
    return lo+((value-lo+delta)%n+n)%n;
}
void ds_date_move(ds_date_t *d, unsigned field, int delta)
{
    if (field==0) d->year=wrap(d->year,2026,2099,delta);
    if (field==1) d->month=wrap(d->month,1,12,delta);
    if (field==2) d->day=wrap(d->day,1,ds_month_days(d->year,d->month),delta);
    if (field==3) d->hour=wrap(d->hour,0,23,delta);
    if (field==4) d->minute=wrap(d->minute,0,59,delta);
    if (d->day>ds_month_days(d->year,d->month)) d->day=ds_month_days(d->year,d->month);
}
int ds_next_node(const ds_entry_t *e, int64_t now)
{
    for (unsigned i=0;i<e->count;i++) if (e->nodes[i].utc>now) return (int)i;
    return -1;
}
static int64_t sort_key(const ds_entry_t *e, int64_t now)
{
    int n=ds_next_node(e,now);
    if (n>=0) return e->nodes[n].utc;
    return e->count ? INT64_MAX-1 : INT64_MAX;
}
unsigned ds_list(unsigned *out, unsigned field, unsigned year, const ds_progress_t *p, int64_t now)
{
    unsigned count=0;
    for (unsigned i=0;i<ds_catalog_count;i++) {
        const ds_entry_t *e=&ds_catalog[i];
        if (e->year!=year || (field>0 && field<5 && e->field!=field) ||
            (field==5 && !(p->flags[i]&1))) continue;
        unsigned j=count++;
        while (j && sort_key(&ds_catalog[out[j-1]],now)>sort_key(e,now)) {out[j]=out[j-1];j--;}
        out[j]=i;
    }
    return count;
}
unsigned ds_checks(uint8_t flags)
{
    unsigned count=0; for(unsigned i=1;i<5;i++) count+=(flags>>i)&1;
    return count;
}
unsigned ds_urgency(int64_t deadline, int64_t now)
{
    if (deadline<=now) return 0;
    int64_t left=deadline-now;
    return left<=86400 ? 1 : left<=7*86400 ? 2 : left<=30*86400 ? 3 : 4;
}
void ds_encode(const ds_progress_t *p, uint8_t b[DS_SAVE_SIZE])
{
    memset(b,0,DS_SAVE_SIZE); b[0]='D';b[1]='S';b[2]=1;
    b[3]=(uint8_t)p->sessions;b[4]=(uint8_t)(p->sessions>>8);
    b[5]=(uint8_t)(p->seed.year-2000);b[6]=(uint8_t)p->seed.month;b[7]=(uint8_t)p->seed.day;
    b[8]=(uint8_t)p->seed.hour;b[9]=(uint8_t)p->seed.minute;b[10]=(uint8_t)ds_catalog_count;
    for(unsigned i=0;i<ds_catalog_count;i++) {
        b[12+i*3]=(uint8_t)ds_catalog[i].id;b[13+i*3]=(uint8_t)(ds_catalog[i].id>>8);b[14+i*3]=p->flags[i]&31;
    }
}
bool ds_decode(ds_progress_t *p, const uint8_t b[DS_SAVE_SIZE])
{
    if (b[0]!='D' || b[1]!='S' || b[2]!=1 || b[10]>DS_MAX_ENTRIES) return false;
    ds_progress_t result={0};
    result.seed=(ds_date_t){b[5]+2000,b[6],b[7],b[8],b[9]};
    if (!ds_date_valid(result.seed)) return false;
    result.sessions=b[3]|(uint16_t)b[4]<<8;
    for(unsigned i=0;i<b[10];i++) {
        unsigned id=b[12+i*3]|(unsigned)b[13+i*3]<<8;
        if(b[14+i*3]&~31) return false;
        for(unsigned j=0;j<ds_catalog_count;j++) if(ds_catalog[j].id==id) result.flags[j]=b[14+i*3];
    }
    *p=result;return true;
}
