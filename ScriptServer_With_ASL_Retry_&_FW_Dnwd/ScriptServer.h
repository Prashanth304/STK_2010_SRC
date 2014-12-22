#ifndef _SCRIPT_SERVER_H_
#define _SCRIPT_SERVER_H_

#include <vector>
#include <map>
#include <istream>
#include <sstream>
#include <stack>

#include "Attribs.h"
#include "Tags.h"
#include "Misc.h"
#include "Csv.h"

#include "tinyxml.h"

struct Cell {
	union {
		int Int ;
		char* Str ;
	};
};


struct Field {
	const char * name ;
	int offset ;
};



class ScriptServer {
public:
	ScriptServer( ) ;
	int RunScript(std::istream*&in) ;

	void GenerateUdoTest(const char *firmwareFileName, int maxBlockSize, int startOffset, int processingTime);

private:
	void generateWaitElementExecResp(TiXmlElement *msg, char *processingTimeBuf, char *udoObjIDBuf);
	void generateWaitElementsIdSfc(TiXmlElement *msg, char *processingTimeBuf, char *reqIDBuf);
	void generateElementsAfterApduUdo(TiXmlElement *msg, const char *ssIpv6, const char *dutUdoIpv6,
			const char *p_udoPort, const char *p_securityPolicy);

	void updateTAIDesync(char * mesg);

protected:
	bool  TaiOffset( std::stringstream& out ) ;
	bool  didx( std::stringstream& out) ;
	bool  load( std::stringstream& out) ;
	bool  GetConfig( std::stringstream& out ) ;
	bool  didxExtdluint( std::stringstream& out ) ;
       char* getBinary(char value);

	bool wait(Params& params, char*& line) ;
	MATCH_TYPE matchLiteral(const char*p, struct Tagwait&w, int policy);
	MATCH_TYPE matchNativeType(const char*p, Tagwait&w, int policy);
	MATCH_TYPE match(char*, struct Tagwait& w, int policy) ;
	bool loadAll(std::vector<struct TagModify>& mvec, char* src, char*& dst, int type, int myType,int& dstSz ) ;
	bool saveAll(std::vector<struct TagModify>& mvec, char* src, int type) ;
	bool readParams(struct Params& params, char*line, char *& outLine,int & outLineSz) ;
	void prepareStack(const char* str );
	void handle(const char* str);

protected:
	bool getOpType( CCsv&, int&) ;
	const char* getMsgType(int) ;
	bool getLayerType( CCsv&, int& type) ;
       bool getStoredCompare(struct Tagwait&w, char *data);
	bool getPolicyType(const char*policy, int& type) ;
	bool getLoop( const char*loop, struct loop_spec& ) ;
	bool getRfNode(const char*rfNode, char*host, int& ackLoggerPort,
			int&backbonePort) ;
	bool parseConfig( ) ;
	bool expandPlaceHolders(const char* line, std::stringstream&) ;

protected:
	typedef bool(ScriptServer::*func_ptr)(std::stringstream& out);
	std::map<const char*, func_ptr, cmp_str> placeholderCallbacks ;
	std::stack<Cell> m_oDataStack ;
} ;

#endif	/* _SCRIPT_SERVER_H_ */
