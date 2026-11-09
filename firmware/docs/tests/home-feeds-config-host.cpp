#include <cassert>
#include "../../src/app_gui/home_feeds_config.h"
int main(){
 using namespace KnobHomeConfig;
 assert(!configured(-1)); assert(!configured(5));
#ifdef TEST_PRIVATE_CONFIG
 assert(configured(0)); assert(!configured(1)); assert(!configured(2)); assert(configured(3));
#else
 for(int i=0;i<5;++i)assert(!configured(i));
#endif
}
