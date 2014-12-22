#ifndef _MISC_H_
#define _MISC_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <vector>
#include <map>
#include <istream>
#include <sstream>

#include "Attribs.h"
#include "Tags.h"
#include "Flog.h"
#include "ConsoleFileSync.h"
#include "Csv.h"

struct cmp_str {
	bool operator()(char const *a, char const *b)
	{
		return strcmp(a, b) < 0 ;
	}
} ;


struct loop_spec {
	int start ;
	int end ;
	int increment ;
	loop_spec() : start(0),end(0),increment(0) {}
};

struct Params {
char *desc;
	char *currentId ;
	int  msgType ;
	char host[256] ;
	int  ackLoggerPort ;
	int  backbonePort ;
	int  timeout ;
	int  policy ;
	struct loop_spec loop ;
	std::vector<struct Tagwait> WaitVec ;
	std::vector<struct TagModify> LoadVec ;
	std::vector<struct TagModify> StoreVec ;
	Params( )
	:msgType(MSG_UNKNOWN)
	,currentId(NULL)
	{
		msgType = MSG_UNKNOWN ;
		currentId=NULL;
	}

} ;


struct Config {
	int  DefaultTimeout ;
	char *InCsvFile ;
	char sec_frac[128];
	char	logFile[256] ;
	int  loopIdx ;
	char LogLevel ;
	std::map<char*, char*,cmp_str> StorageMap ;
	Config()
		: InCsvFile(NULL)
		, DefaultTimeout(600)
		, LogLevel(CFLog::LL_ERROR|CFLog::LL_DEBUG|CFLog::LL_INFO)
	{
	}
} ;

bool  getMsgType(CCsv&, int&/*, bool log=true*/) ;
bool  getMsgType(const char*, int&/*, bool log=true*/) ;
void  diep(char const *s) ;
int   sendline(Params params, std::stringstream& line) ;
char* szNow(void) ;




extern Config g_oCfg ;

#endif
