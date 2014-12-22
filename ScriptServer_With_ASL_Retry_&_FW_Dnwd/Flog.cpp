/*
 * CFLog.cpp
 *
 *  Created on: Mar 22, 2010
 *      Author: mariusn
 */

#include <sys/types.h>
#include <sys/stat.h>
#include "Flog.h"

#include <stdio.h>
#include <stdarg.h>

CFLog::CFLog()
	: m_eLogLevel(LL_INFO)
	, m_pLogSink(NULL)
{
	// TODO Auto-generated constructor stub
}

CFLog::~CFLog()
{
	// TODO Auto-generated destructor stub
}

void CFLog::WriteMsg(const std::ostream& message)
{
	m_oStrStream << message.rdbuf() ;
	m_pLogSink->consume(m_oStrStream);
	m_oStrStream.str("");
}
void CFLog::WriteMsg(const char* message, ... )
{
	va_list ap;
	va_start(ap, message);
	m_pLogSink->consume( message, ap  );
	va_end(ap);
}
CFLog g_stFlog;
