#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "../../src/app_gui/home_feeds_parse.h"
using namespace KnobHomeFeeds;
int main(int argc,char** argv){
 const std::string warning="<br />\n<b>Deprecated</b>:  explode(): Passing null to parameter #2 ($string) of type string is deprecated in <b>/home/www/emon.boxtec.ch/web/locale.php</b> on line <b>43</b><br />\n";
 auto good=[](const std::string& s,double expected){Reading r;assert(parse(s.data(),s.size(),r));assert(r.valid&&r.value==expected&&r.sourceTime==1793178717);};
 auto bad=[](const std::string& s){Reading r;r.value=123;r.valid=false;assert(!parse(s.data(),s.size(),r));assert(!r.valid&&r.value==123);};
 good("{\"time\":1793178717,\"value\":-22.81}",-22.81);
 good(warning+"{\"time\":1793178717,\"value\":-22.81}",-22.81);
 good(" \r\n"+warning+"{\"time\":1793178717,\"value\":0}\r\n",0);
 good(warning+"{\"time\":1793178717,\"value\":1}",1);
 bad(warning);bad("<html>Login</html>");bad("Error {\"time\":1793178717,\"value\":1}");
 bad(warning+"{\"time\":1793178717,\"value\":null}");
 bad(warning+"{\"time\":1793178717,\"value\":\"1\"}");
 bad(warning+"{\"time\":1793178717}");bad(warning+"{\"value\":1}");
 bad(warning+"{\"time\":-1,\"value\":1}");bad(warning+"{\"time\":1,\"value\":1}");
 bad(warning+"{\"time\":1793178717,\"value\":1");
 bad(warning+"{\"time\":1793178717,\"value\":1}ERROR");
 bad(warning+"{\"time\":1793178717,\"value\":1}{}");
 Reading r;assert(!parse(nullptr,0,r));
 if(argc>1){std::ifstream f(argv[1]);std::stringstream stream;stream<<f.rdbuf();auto s=stream.str();assert(!s.empty());assert(parse(s.data(),s.size(),r));printf("Live response parsed: %.2f (%zu bytes)\n",r.value,s.size());}
 puts("PASS: normal JSON, known PHP prefix, zero/one, whitespace and invalid responses.");
}
