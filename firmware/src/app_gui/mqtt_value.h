#pragma once
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
namespace KnobMQTT {
// A finite decimal number only; no JSON objects, quoted strings, hex or NaN.
inline bool parseValue(const uint8_t* payload,size_t length,double& result) {
    if(!payload || !length || length>48)return false;
    char text[49];
    for(size_t i=0;i<length;++i) {
        if(payload[i]==0 || payload[i]>127)return false;
        text[i]=static_cast<char>(payload[i]);
    }
    text[length]=0;
    const char* p=text;
    while(isspace(static_cast<unsigned char>(*p)))++p;
    if(*p=='-'||*p=='+')++p;
    unsigned digits=0;
    while(*p>='0'&&*p<='9'){++digits;++p;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){++digits;++p;}}
    if(!digits)return false;
    if(*p=='e'||*p=='E') {
        ++p;if(*p=='-'||*p=='+')++p;
        unsigned exponentDigits=0;
        while(*p>='0'&&*p<='9'){++exponentDigits;++p;}
        if(!exponentDigits)return false;
    }
    while(isspace(static_cast<unsigned char>(*p)))++p;
    if(*p)return false;
    errno=0;char* end=nullptr;
    const double value=strtod(text,&end);
    if(errno==ERANGE||!isfinite(value)||fabs(value)>=1000000)return false;
    result=value;return true;
}
inline bool fresh(uint32_t now,uint32_t at,uint32_t limit) {return now-at<=limit;}
}
