
class CConsoleFileSink : public CFLogSink {
public:
	CConsoleFileSink(const char* path, const char*mode="w")
	{
		m_pFile =fopen( path, mode );
		setlinebuf(m_pFile);
	}
	virtual ~CConsoleFileSink() {}
public:
	bool dissociate( )
	{
		if ( m_pFile ) fclose(m_pFile);
	}
	void consume( const std::ostringstream& msg  )
	{
		printf( "%s", msg.str().c_str() );
		fprintf( m_pFile, "%s", msg.str().c_str() );
	}
	void consume( const char* message, va_list ap  )
	{
		vprintf( message, ap );
		vfprintf( m_pFile, message, ap );
	}
protected:
	FILE* m_pFile;
};


