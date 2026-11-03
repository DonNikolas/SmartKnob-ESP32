#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include "home_feeds.h"
namespace KnobHomeFeeds {
namespace ParseDetail {
inline bool whitespace(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
inline void trimStart(const char*& data,size_t& length){
    while(length&&whitespace(*data)){++data;--length;}
}
// Work around only the specific Boxtec locale.php warning observed on
// 2026-10-28. Do not search arbitrary HTML/error pages for a JSON object.
inline bool skipBoxtecWarning(const char*& data,size_t& length){
    trimStart(data,length);
    static const char warning[]=
        "<br />\n<b>Deprecated</b>:  explode(): Passing null to parameter #2 ($string) of type string is deprecated in "
        "<b>/home/www/emon.boxtec.ch/web/locale.php</b> on line <b>43</b><br />";
    const size_t size=sizeof(warning)-1;
    if(length>=size&&memcmp(data,warning,size)==0){data+=size;length-=size;trimStart(data,length);}
    return length&&*data=='{';
}
// ArduinoJson's reader interface lets us check that no trailing error page,
// second JSON object, or other non-whitespace bytes follow the measurement.
struct Reader {
    const char* data;size_t length,position;
    int read(){return position<length?static_cast<unsigned char>(data[position++]):-1;}
    size_t readBytes(char* buffer,size_t requested){
        size_t count=requested<length-position?requested:length-position;
        memcpy(buffer,data+position,count);position+=count;return count;
    }
};
}
inline bool parse(const char* data,size_t length,Reading& out){
    if(!data||!length||!ParseDetail::skipBoxtecWarning(data,length))return false;
    StaticJsonDocument<256> doc;
    ParseDetail::Reader input={data,length,0};
    if(deserializeJson(doc,input)||!doc.is<JsonObject>())return false;
    for(size_t i=input.position;i<length;++i)if(!ParseDetail::whitespace(data[i]))return false;
    // Numeric strings/null/errors are not measurements. In particular null must
    // never silently become zero, and a successful fetch is not a fresh sample.
    if(!doc["value"].is<double>()||!doc["time"].is<uint32_t>())return false;
    double value=doc["value"].as<double>();uint32_t time=doc["time"];
    if(!isfinite(value)||time<1577836800UL)return false;
    out.value=value;out.sourceTime=time;out.valid=true;return true;
}
}
