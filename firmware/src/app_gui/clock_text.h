#pragma once
#include <time.h>
#include <stdio.h>
namespace KnobClock {
// Europe/Zurich: CET/CEST. Work from UTC without changing the process-wide TZ,
// which is also used by the existing Radar SNTP setup.
inline int weekday(int year,int month,int day){
    static const int offsets[]={0,3,2,5,0,3,5,1,4,6,2,4};
    if(month<3)--year;
    return (year+year/4-year/100+year/400+offsets[month-1]+day)%7;
}
inline int offsetSeconds(const tm& utc){
    const int year=utc.tm_year+1900,month=utc.tm_mon+1;
    const int marchSunday=31-weekday(year,3,31),octoberSunday=31-weekday(year,10,31);
    bool summer=month>3&&month<10;
    if(month==3)summer=utc.tm_mday>marchSunday||(utc.tm_mday==marchSunday&&utc.tm_hour>=1);
    if(month==10)summer=utc.tm_mday<octoberSunday||(utc.tm_mday==octoberSunday&&utc.tm_hour<1);
    return summer?7200:3600;
}
inline void format(char* out,size_t size,time_t utcSeconds){
    tm utc={},local={};
    if(utcSeconds<1700000000 || !gmtime_r(&utcSeconds,&utc)){snprintf(out,size,"--:--");return;}
    time_t localSeconds=utcSeconds+offsetSeconds(utc);
    if(!gmtime_r(&localSeconds,&local)){snprintf(out,size,"--:--");return;}
    snprintf(out,size,"%02d:%02d",local.tm_hour,local.tm_min);
}
}
