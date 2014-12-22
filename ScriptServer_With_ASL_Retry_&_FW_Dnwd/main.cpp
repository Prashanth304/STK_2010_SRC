#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include "ScriptServer.h"
#define VERSION "2.3.5.3"

char	*g_InFile   =NULL;
char *firmwareFileName = 0;

////////////////////////////////////////////////////////////////////////////////
static void usage()
{
	printf( "Version : "VERSION "\n" \
	        "script_server [OPTIONS]\n" \
	        "	 -f   <XML_FILE>	Input file.\n"
	        "	 -o   <OUT_FILE>	Output file.\n"
	        "	 -t   <TIMEOUT>		Timeout to wait for each response.\n"
	        "	 -l   <LOG_LEVEL>	Log level: 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG. Default level used is INFO.\n"
	        "	 -v             	Print Version\n"
	        "	 -u   <FIRMWARE_FILE [MAX_BLOCK_SIZE DATA_OFFSET PROCESSING_TIME]>	UDO specific option. Needed input: firmware file name. Optional parameters: maximum block size, data offset in file, processing time for a packet on DUT.\n"
	      );
	exit(1);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	int c;
	int optionsCount = 0; //used to exit when an option cannot be used together with other options; eg: "-f -u"
	while ( -1 != (c=getopt(argc, argv, "hf:o:t:l:vu:")) )
	{
		switch (c)
		{
		case 'f':
			g_InFile = optarg ;
			++optionsCount;
			break;
		case 'o':
			strcpy( g_oCfg.logFile,optarg) ;
			++optionsCount;
			break;
		case 't':
			g_oCfg.DefaultTimeout = atoi(optarg);
			++optionsCount;
			break ;
		case 'l':
			g_stFlog.SetLogLevel( CFLog::LogLevel(atoi(optarg)) );
			++optionsCount;
			break ;
		case 'u':
		{
			if (optionsCount) {
				printf("Invalid usage: option -%c must not be combined with other options.\n", optopt);
				return 1;
			}
			firmwareFileName = optarg;
			int maxBlockSize = 64; //when not specified by user, default will be used
			int startOffset = 0; //offset where first block starts in the firmware file; default = 0;
			int processingTime = 0; //packet processing time on DUT; influences the delay between two packets; default value = 0 - meaning the next block is sent as soon as the response for the previous is received

			for (int index = optind; index < argc; index++) {
				printf ("Non-option argument %s \n", argv[index]);
			}

			int index = optind;
			if (index < argc) {
				maxBlockSize = atoi(argv[index]);
				if (++index < argc) {
					startOffset = atoi(argv[index]);
					if (++index < argc) {
						processingTime = atoi(argv[index]);
					}
				}
			}
			printf("Parameters: maxBlockSize=%d startOffset=%d processingTime=%d \n", maxBlockSize, startOffset, processingTime);

			ScriptServer scriptServer;
			scriptServer.GenerateUdoTest(firmwareFileName, maxBlockSize, startOffset, processingTime);

			printf("script_server exit");
			/// nothing else to be done
			return 0;
		}
		case 'v':
			printf("Version : "VERSION"\n");
			exit(0);
		case '?':
			//printf("Error - No such option: `%c'\n\n", optopt);
            if (optopt == 'f' || optopt == 'o' || optopt == 't' || optopt == 'l' || optopt == 'u') {
            	fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            }
            else if (isprint (optopt)) {
            	fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            }
            else {
            	fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            }
            usage(); //return 1;
		case ':':
			printf("Error - Option `%c' needs a value\n\n", optopt);
		case 'h':
		default:
			usage();
		}
	}

	if ( !g_InFile )
	{
		printf("Error - No XML_FILE specified\n");
		usage();
	}
	{
		g_oCfg.InCsvFile = strdup(g_InFile);
		char* p=strchr(g_oCfg.InCsvFile,'.');
		if (p)
		{
			*p=0;
			sprintf(g_oCfg.logFile, "%s.log", g_oCfg.InCsvFile );
			sprintf(g_oCfg.InCsvFile, "%s.csv", g_oCfg.InCsvFile );
			g_stFlog.LogSink( new CConsoleFileSink(g_oCfg.logFile) );
		}
	}
	LOG_INFO("Entered "<<"the "<<"scriptserver\n");
	char cmd[1024];
	sprintf( cmd, "sabcmd ../../Config/tocsv.xsl %s %s", g_InFile, g_oCfg.InCsvFile );
	system(cmd);

	std::fstream filestr;
	std::istream* in;

	if ( g_InFile[0] == '-' )
		in = &std::cin ;
	else
	{
		filestr.open( g_oCfg.InCsvFile, std::fstream::in );
		if ( filestr.fail() )
		{
			printf("Error - Failed to open input file [%s]\n", argv[optind]);
			exit(1);
		}
		in = &filestr ;
	}
	ScriptServer ss ;
	ss.RunScript(in);

	if (filestr.is_open())
		filestr.close();
	unlink( g_oCfg.InCsvFile ); //erase csv file from disk
	return 0;	//Success
}
