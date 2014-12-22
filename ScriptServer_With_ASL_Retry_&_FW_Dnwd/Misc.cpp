#include "Misc.h"
#include "SimpleIni.h"

Config g_oCfg ;



////////////////////////////////////////////////////////////////////////////////
/// @brief Get the time as string.
////////////////////////////////////////////////////////////////////////////////
char * szNow(void)
{
	static char now[64] ;
	struct timeval tv ;
	gettimeofday(&tv, (struct timezone*) 0 /*NULL*/) ;
	struct tm * pTm = gmtime(&tv.tv_sec) ;

	sprintf(now, "%04d-%02d-%02d %02d:%02d:%02d.%03u", pTm->tm_year + 1900,
			pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_hour,
			pTm->tm_min, pTm->tm_sec, (unsigned) tv.tv_usec / 1000) ;

	return now ;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////
void diep(char const *s)
{
	perror(s) ;
	exit(1) ;
}


void getTaiTime()
{
	struct timeval tv ;
	gettimeofday(&tv, (struct timezone*)NULL) ;
	sprintf( g_oCfg.sec_frac, ",%04lu,%04lu", tv.tv_sec+(0x16925E80+34), tv.tv_usec&0x00FFFFFF );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Send a line on UDP.
////////////////////////////////////////////////////////////////////////////////
int sendline(Params params, std::stringstream& line)
{
	int rv ;
	if ( !line )
	{
		LOG_ERROR( "Error - sendline: null line\n") ;
		return false ;
	}

	struct sockaddr_in si_other ;
	int s, slen = sizeof(si_other) ;
	if ( (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 )
		diep("socket") ;

	memset((char *) &si_other, 0, sizeof(si_other)) ;
	si_other.sin_family = AF_INET ;
	si_other.sin_port = htons(params.backbonePort) ;
	if ( 0 == inet_aton(params.host, &si_other.sin_addr) )
	{
		LOG_ERROR( "Error - inet_aton(host:%s,port:%i) failed\n", params.host, params.backbonePort ) ;
		exit(1) ;
	}
	struct sockaddr_in servaddr ;
	bzero(&servaddr, sizeof(servaddr)) ;
	servaddr.sin_family = AF_INET ;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY) ;
	servaddr.sin_port = htons(params.backbonePort) ;
	rv = bind(s, (struct sockaddr *) &servaddr, sizeof(servaddr)) ;
	if( 0 != rv )
	{
		LOG_ERROR("Error - Unable to bind(port:%u)", params.backbonePort ) ;
		exit(1);
	}

	int type ;
	::getMsgType(line.str().c_str(), type/*, false*/);
	if ( type == TX_RF || type==TX_CFG || type==TX_RSP )
	{
		getTaiTime();
		line << g_oCfg.sec_frac ;
	}
	if ( sendto(s, line.str().c_str(), line.str().length(), 0, (const sockaddr*) &si_other, slen)
			== -1 )
		diep("sendto()") ;
	else
		LOG_INFO("\tSENT UDP: [%s]: [host:%s] [port:%d] [%s]\n", szNow(), params.host, params.backbonePort, line.str().c_str() ) ;

	close(s) ;
	return true ;
}

