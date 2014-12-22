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
#include <map>
#include <iomanip>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <deque>

#include "Misc.h"

#include "SimpleIni.h"
#include "ScriptServer.h"

struct RfNode {
	char* host;
	int ackLoggerPort;
	int backbonePort;
};
std::map<const char*,RfNode,cmp_str> RfNodeMap ;

///names of mandatory variables in ss.ini
///used to generate UDO test .xml
const char *ssIpv6 = "SS_IPV6Add_PORT";
const char *dutUdoIpv6 = "DUT1_IPV6Add";
const char *rfNode = "RF_test_point1";

const char *udoPort = "UDO_PORT";
const char *udoObjectID = "UDO_OBJECT_ID";
const char *securityPolicy = "SECURITY_POLICY"; //"0"= no encryption, "5"= encryption enabled

///list of latest 3 TAI desynchronization values, computed based on TAI received in RX_RF messages
std::deque<int> taiDesyncQ;

////////////////////////////////////////////////////////////////////////////////
int offset( const enum FIELD_TYPE* lt, const FIELD_TYPE needle )
{
	for ( unsigned i=0; lt[i] != INT_MAX; ++i )
	{
		if ( lt[i] == needle )
		{
			return i;
		}
	}
	return -1;
}
char * searchComma( char* dst, int comma)
{
	char *pdst = (char*) dst ;
	for ( int k = 0; k < comma; ++k)
	{
		pdst = strchr(pdst, ',') ;
		if ( !pdst )
		{
			LOG_INFO( "Error - Mod(%i):Malformed dst CSV line[%s]\n", k, dst) ;
			return NULL ;
		}
		if ( *pdst == ',' )
			pdst++ ;
	}
	return pdst ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the type of a message
/// @param c	Csv object from which the message string is read
/// @param type	Message type
/// @retval false when message type could not be determined
////////////////////////////////////////////////////////////////////////////////
bool getMsgType(CCsv& c, int& type /*,bool log*/){
	std::string op;
	c.Get(op);
	const char* msg=op.c_str();
//LOG_INFO( "This is called first Message %s \n",msg) ;
	return getMsgType(msg, type /*,log*/);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the type of a message
/// @param msg	Message string
/// @param type	Message type
/// @retval false when message type could not be determined
////////////////////////////////////////////////////////////////////////////////
bool getMsgType(const char *msg, int& type/*, bool log*/)
{
//LOG_INFO( "This is called Second Message%s \n",msg) ;
	if ( !msg ) return MSG_UNKNOWN ;

	for ( unsigned i = 0; g_MsgTypes[i].tokenName; ++i)
		if ( !strncmp(msg, g_MsgTypes[i].tokenName,g_MsgTypes[i].tokenLength)  )
		{
			type = g_MsgTypes[i].type ;
			return true ;
		}

	if (  0!=msg[0] ) LOG_ERROR( "[msg type unknown:%s,%i] ",msg,msg[0]) ;
	type = MSG_UNKNOWN ;
	return false ;
}


char desc1[450]; //Store descreption

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ScriptServer::ScriptServer( )
{
	placeholderCallbacks["TAIOFFSET"] = &ScriptServer::TaiOffset ;
	placeholderCallbacks["DIDX"]      = &ScriptServer::didx ;
	placeholderCallbacks["DIDX_EXTDLUINT"] = &ScriptServer::didxExtdluint ;
	placeholderCallbacks["LOAD"]      = &ScriptServer::load ;
}

////////////////////////////////////////////////////////////////////////////////
/// @author sorin bidian
/// @brief Read data from firmware file and generate XML file containing test messages for UDO
/// @param firmwareFileName	Firmware image to be downloaded
/// @param maxBlockSize		Maximum block size, specified by user
/// @param startOffset		Offset of the first data block in the firmware file
/// @param processingTime	Time needed by DUT for processing a block, specified by user
/// @remarks Steps: check that the needed config keys are defined, read data from file, generate xml messages - Start Download, Download Data, End Download, Apply
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::GenerateUdoTest(const char *firmwareFileName, int maxBlockSize, int startOffset, int processingTime) {
//sample test code
//	const char* demoStart =
//		"<?xml version=\"1.0\"  standalone='no' >\n"
//		"<!-- Our to do list data -->"
//		"<ToDo>\n"
//		"<!-- Do I need a secure PDA? -->\n"
//		"<Item priority=\"1\" distance='close'> Go to the <bold>Toy store!</bold></Item>"
//		"<Item priority=\"2\" distance='none'> Do bills   </Item>"
//		"<Item priority=\"2\" distance='far &amp; back'> Look for Evil Dinosaurs! </Item>"
//		"</ToDo>";
//	{
//		TiXmlDocument doc( "demotest.xml" );
//		doc.Parse( demoStart );
//
//		if ( doc.Error() )
//		{
//			printf( "Error in %s: %s\n", doc.Value(), doc.ErrorDesc() );
//			exit( 1 );
//		}
//		doc.SaveFile();
//	}

	///make sure that UDO placeholders are present in ss.ini with the expected name
	CSimpleIniA ini(false, true, true);

	SI_Error rv = ini.LoadFile("../../Config/ss.ini");
	if (rv != 0) {
		printf("Config file <ss.ini> not found (err %i)\n", rv);
		//LOG_ERROR("Config file <ss.ini> not found (err %i)\n", rv);
		return;
	}

	bool checkFailed = false;

	const char *p_ss = ini.GetValue("EXPORT", ssIpv6);
	if(!p_ss) {
		checkFailed = true; ///"SS_IPV6Add_PORT" key not found in "EXPORT" section
		printf("The following key has to be defined in ss.ini > [EXPORT] section: %s\n", ssIpv6);
		//LOG_INFO("The following key has to be defined in ss.ini > [EXPORT] section: SS_IPV6Add_PORT\n");
	}

	const char *p_dut = ini.GetValue("EXPORT", dutUdoIpv6);
	if(!p_dut) {
		checkFailed = true; ///"DUT1_IPV6Add" key not found in "EXPORT" section
		printf("The following key has to be defined in ss.ini > [EXPORT] section: %s\n", dutUdoIpv6);
		//LOG_INFO("The following key has to be defined in ss.ini > [EXPORT] section: DUT1_IPV6Add\n");
	}

	const char *p_node = ini.GetValue("RF_NODES", rfNode);
	if(!p_node) {
		checkFailed = true; ///RF_test_point1" key not found in "RF_NODES" section
		printf("The following key has to be defined in ss.ini > [RF_NODES] section: %s\n", rfNode);
		//LOG_INFO("The following key has to be defined in ss.ini > [RF_NODES] section: RF_UDO_test_point\n");
	}

	const char *p_udoPort = ini.GetValue("EXPORT", udoPort);
	if(!p_udoPort) {
		checkFailed = true; ///"UDO_port" key not found in "EXPORT" section
		printf("The following key has to be defined in ss.ini > [EXPORT] section: %s\n", udoPort);
	}

	const char *p_udoObjectID = ini.GetValue("EXPORT", udoObjectID);
	if(!p_udoObjectID) {
		checkFailed = true; ///"UDO_objectID" key not found in "EXPORT" section
		printf("The following key has to be defined in ss.ini > [EXPORT] section: %s\n", udoObjectID);
	}

	const char *p_securityPolicy = ini.GetValue("EXPORT", securityPolicy); //securityPolicy is optional; p_securityPolicy has to be checked for NULL when used

	///continue only if we have all needed placeholders defined in config
	if(checkFailed) {
		return;
	}

	///open firmware file - in binary mode
	TIXML_STRING filename(firmwareFileName);

	FILE* file = TiXmlFOpen(firmwareFileName, "rb");
	if (!file) {
		printf("Error: could not open file %s \n", firmwareFileName);
		return;
	}

	///get the data size
	long length = 0;
	fseek(file, 0, SEEK_END);
	length = ftell(file) - startOffset;
	fseek(file, startOffset, SEEK_SET);

	if (length <= 0) {
		printf("Error in establishing data size \n");
		fclose(file);
		return;
	}

	printf("File length=%d \n", length);

	unsigned char *buf = new (std::nothrow) unsigned char[length];
	if (buf == 0) {
		printf("ERROR memory could not be allocated to buffer the file. File too large: %d \n", length);
		LOG_INFO("ERROR memory could not be allocated to buffer the file. File too large: %d \n", length);
		return;
	}
	memset(buf, 0, length);

	///copy the data from file into the buffer
	size_t res = fread( buf, 1, length, file );
	//size_t res = fread( buf, length, 1, file ); //this reads one element of size=length

	if ( res != length ) {
		delete [] buf;
		printf("Reading file error \n");
		fclose(file);
		return;
	}

	///the file can be closed now
	fclose(file);

	///testing print
	//	printf("[SORIN] buf:");
	//	for (int i=0; i < length ; i++) {
	//		printf("%02X ", buf[i]);
	//		if (i % 50 == 0) {
	//			printf("\n");
	//		}
	//	}
	//	printf("\n");

	///generate xml file and add messages
	TiXmlDocument doc;
	TiXmlDeclaration *decl = new TiXmlDeclaration( "1.0", "", "" );
	doc.LinkEndChild(decl);

	TiXmlComment *comment = new TiXmlComment();
	comment->SetValue("UDO test");
	doc.LinkEndChild(comment);

	TiXmlElement *root = new TiXmlElement( "MsgList" );
	doc.LinkEndChild(root);

	TiXmlElement *element = 0;

	int msgNumber = 0;
	char msgNoBuf[8] = {0};
	unsigned char reqID = 0; ///uint8
	char reqIDBuf[4] = {0}; //used in <Wait> elements to match the reqID
	char processingTimeBuf[4] = {0}; //used to generate timeout = processingTime + 10
	sprintf(processingTimeBuf, "%d", processingTime + 10);
	char udoObjIDBuf[2] = {0};
	sprintf(udoObjIDBuf, "%s", p_udoObjectID); //DMAP UDO ID=8, UAP UDO ID=7

	///--- MSG 1 - StartDownload req
	TiXmlElement *msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "norecv");
	root->LinkEndChild(msg);

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("Start Download"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF"));
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);

	element = new TiXmlElement("APDU");
	char apdu[40] = {0};
	//05=exec_req  1=SOID  8=UDO_ID  req_id  01=meth_id size  payload
	//sprintf(apdu, "0518 %02X 01 07 %04X %08X 00", ++reqID, (unsigned short int) maxBlockSize, (unsigned int) length);
	sprintf(apdu, "051%s %02X 01 07 %04X %08X 00", udoObjIDBuf, ++reqID, (unsigned short int) maxBlockSize, (unsigned int) length);
	element->LinkEndChild(new TiXmlText(apdu));
	msg->LinkEndChild(element);

	generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);






	///---- MSG 2 - DownloadData [ first block ]
	/// wait for StartDownload response
	msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "nomatchdrop|nosend");//RKP: Wait for 30 Sec changed from nomatchdrop to wait;
	root->LinkEndChild(msg);

	generateWaitElementExecResp(msg, processingTimeBuf, udoObjIDBuf);
	sprintf(reqIDBuf, "%02X", reqID);
	generateWaitElementsIdSfc(msg, processingTimeBuf, reqIDBuf); ///match for reqID and sfc

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("Download Data"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF"));
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);


	element = new TiXmlElement("APDU");
	element->LinkEndChild(new TiXmlText(" "));
	msg->LinkEndChild(element);
	
	generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);



	int currentBlockNumber = 0; ///uint16


	///---- MSG 2 - DownloadData [ first block ]
	/// wait for StartDownload response
	msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "wait");//RKP: Wait for 30 Sec changed from nomatchdrop to wait;
	root->LinkEndChild(msg);

	generateWaitElementExecResp(msg, processingTimeBuf, udoObjIDBuf);
	sprintf(reqIDBuf, "%02X", reqID);
	generateWaitElementsIdSfc(msg, processingTimeBuf, reqIDBuf); ///match for reqID and sfc

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("Download Data"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF"));
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);

	++currentBlockNumber;
	///data size may be smaller than maxBlockSize (this means one block transfer)
	if (length < maxBlockSize) {
		maxBlockSize = length;
	}

	printf("Reading blocks.. \n");

	element = new TiXmlElement("APDU");
	///05, soid/doid,  req_id, MethID, Size, APDU - 0518 03 02 sz apdu(blockNo + block)
	int headerSize = 14 + 2*2 + 1; //10=header(spaces included) + 2*2=block number
	char *apduBlock = new (std::nothrow) char[headerSize + maxBlockSize*2]; //every char from block is 2 digits -> 2 chars in xml apdu
	if (apduBlock == 0) {
		printf("ERROR memory could not be allocated to buffer a block. \n");
		LOG_INFO("ERROR memory could not be allocated to buffer a block. \n");
		return;
	}
	memset(apduBlock, 0, headerSize + maxBlockSize*2);
	//sprintf(apduBlock, "0518 %02X 02 %02X %04X ", ++reqID, 2 + maxBlockSize, currentBlockNumber); ///sizeof(DownloadData apdu) = sizeof(blockNumber) + sizeof(block)
	sprintf(apduBlock, "051%s %02X 02 %02X %04X ", udoObjIDBuf, ++reqID, 2 + maxBlockSize, currentBlockNumber); ///sizeof(DownloadData apdu) = sizeof(blockNumber) + sizeof(block)
	//memcpy(apduBlock + headerSize, buf + (currentBlockNumber - 1) * maxBlockSize, maxBlockSize); ///not good - we need hexadecimal representation
	int index = 0;
	for (int i = 0; i < maxBlockSize; i++) {
		int pos = (currentBlockNumber - 1) * maxBlockSize + i;
		//printf("[SORIN] pos=%d %02X index=%d\n", pos, buf[pos], index);
		index = sprintf(apduBlock + headerSize + i*index, "%02X", buf[pos]);
	}
	element->LinkEndChild(new TiXmlText(apduBlock));
	msg->LinkEndChild(element);
	delete [] apduBlock;

	generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);

	///---- MSG 3..n - DownloadData [ remaining blocks ]
	/// wait for response to previous DownloadData request
	int remainingDataSize = length - currentBlockNumber * maxBlockSize;
	while (remainingDataSize > 0) {
		///block may be smaller than maxBlockSize (last block)
		int currentBlockSize = remainingDataSize < maxBlockSize ? remainingDataSize : maxBlockSize;

		//printf("\tcurrentBlock=%4d size=%d \n", currentBlockNumber, currentBlockSize);

		msg = new TiXmlElement("MSG");
		msg->SetAttribute("policy", "nomatchdrop");
		root->LinkEndChild(msg);

		generateWaitElementExecResp(msg, processingTimeBuf, udoObjIDBuf);
		sprintf(reqIDBuf, "%02X", reqID);
		generateWaitElementsIdSfc(msg, processingTimeBuf, reqIDBuf); ///match for reqID and sfc

		element = new TiXmlElement("Description");
		element->LinkEndChild(new TiXmlText("Download Data"));
		msg->LinkEndChild(element);

		element = new TiXmlElement("RFNode");
		//element->LinkEndChild(new TiXmlText(RF_test_point1"));
		element->LinkEndChild(new TiXmlText(rfNode));
		msg->LinkEndChild(element);

		element = new TiXmlElement("Type");
		element->LinkEndChild(new TiXmlText("TX_RF"));
		msg->LinkEndChild(element);

		++msgNumber;
		element = new TiXmlElement("MsgNo");
		sprintf(msgNoBuf, "%d", msgNumber);
		element->LinkEndChild(new TiXmlText(msgNoBuf));
		msg->LinkEndChild(element);

		++currentBlockNumber;

		element = new TiXmlElement("APDU");
		char *apduBlock = new (std::nothrow) char[headerSize + currentBlockSize*2]; //every char from block is 2 digits -> 2 chars in xml apdu
		if (apduBlock == 0) {
			printf("ERROR memory could not be allocated to buffer a block. \n");
			LOG_INFO("ERROR memory could not be allocated to buffer a block. \n");
			return;
		}
		memset(apduBlock, 0, headerSize + currentBlockSize*2);
		//sprintf(apduBlock, "0518 %02X 02 %02X %04X ", ++reqID, 2 + currentBlockSize, currentBlockNumber); ///sizeof(DownloadData apdu) = sizeof(blockNumber) + sizeof(block)
		sprintf(apduBlock, "051%s %02X 02 %02X %04X ", udoObjIDBuf, ++reqID, 2 + currentBlockSize, currentBlockNumber); ///sizeof(DownloadData apdu) = sizeof(blockNumber) + sizeof(block)
		index = 0;
		for (int i = 0; i < currentBlockSize; i++) {
			int pos = (currentBlockNumber - 1) * maxBlockSize + i;
			index = sprintf(apduBlock + headerSize + i*index, "%02X", buf[pos]);
			//printf("%s \n", apduBlock);
		}
		element->LinkEndChild(new TiXmlText(apduBlock));
		msg->LinkEndChild(element);
		delete [] apduBlock;

		generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);

		remainingDataSize = length - (currentBlockNumber - 1) * maxBlockSize - currentBlockSize;
	}

	printf("\tlastBlock=%4d size=%d \n", currentBlockNumber - 1, length - (currentBlockNumber - 1) * maxBlockSize);

	///---- MSG - End Download
	msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "nomatchdrop");
	root->LinkEndChild(msg);

	generateWaitElementExecResp(msg, processingTimeBuf, udoObjIDBuf);
	sprintf(reqIDBuf, "%02X", reqID);
	generateWaitElementsIdSfc(msg, processingTimeBuf, reqIDBuf); ///match for reqID and sfc

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("End Download"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF"));
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);

	element = new TiXmlElement("APDU");
	//05=exec_req  1=SOID  7=UDO_ID  req_id  meth_id  size  payload=00(success)
	//sprintf(apdu, "0518 %02X 03 01 00", ++reqID);
	sprintf(apdu, "051%s %02X 03 01 00", udoObjIDBuf, ++reqID);
	element->LinkEndChild(new TiXmlText(apdu));
	msg->LinkEndChild(element);

	generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);

	///---- MSG - Apply
	msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "nomatchdrop");
	root->LinkEndChild(msg);

	generateWaitElementExecResp(msg, processingTimeBuf, udoObjIDBuf);
	sprintf(reqIDBuf, "%02X", reqID);
	generateWaitElementsIdSfc(msg, processingTimeBuf, reqIDBuf); ///match for reqID and sfc

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("Apply"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF"));
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);

	element = new TiXmlElement("APDU");
	//04=write_req  1=SOID  7=UDO_ID  req_id  attr_id  size  payload=01(apply)
	//sprintf(apdu, "0418 %02X 04 01 01", ++reqID);
	sprintf(apdu, "041%s %02X 04 01 01", udoObjIDBuf, ++reqID);
	element->LinkEndChild(new TiXmlText(apdu));
	msg->LinkEndChild(element);

	generateElementsAfterApduUdo(msg, ssIpv6, dutUdoIpv6, p_udoPort, p_securityPolicy);


	//Wait for 400 sec for Apply command
	//char processingTimeBuf_ForApply[4] = {0}; 
	//sprintf(processingTimeBuf, "%d", 400);
	
	sprintf(processingTimeBuf, "%d", 400);

	///---- MSG - wait apply response
	msg = new TiXmlElement("MSG");
	msg->SetAttribute("policy", "nomatchdrop|nosend");
	root->LinkEndChild(msg);

	element = new TiXmlElement("Wait");
	element->SetAttribute("timeout", processingTimeBuf);
	msg->LinkEndChild(element);
	TiXmlElement *innerElement = 0;
	innerElement = new TiXmlElement("Match");
	innerElement->SetAttribute("type", "RX_RF");
	innerElement->SetAttribute("layer", "APP");
	innerElement->SetAttribute("offset", "0");
	innerElement->SetAttribute("size", "4");
	//innerElement->LinkEndChild(new TiXmlText("8481")); ///match for write resp = 84, SOID/DOID = 81
	char matchArray[5] = {0};
	sprintf(matchArray, "84%s1", udoObjIDBuf);
	innerElement->LinkEndChild(new TiXmlText(matchArray));
	element->LinkEndChild(innerElement);

	element = new TiXmlElement("Wait");
	element->SetAttribute("timeout", processingTimeBuf);
	msg->LinkEndChild(element);
	innerElement = new TiXmlElement("Match");
	innerElement->SetAttribute("type", "RX_RF");
	innerElement->SetAttribute("layer", "APP");
	innerElement->SetAttribute("offset", "8");
	innerElement->SetAttribute("size", "2");
	innerElement->LinkEndChild(new TiXmlText("22")); ///match for sfc - operationAccepted = 34
	element->LinkEndChild(innerElement);

	element = new TiXmlElement("Description");
	element->LinkEndChild(new TiXmlText("Wait Apply response"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("RFNode");
	//element->LinkEndChild(new TiXmlText(RF_test_point1"));
	element->LinkEndChild(new TiXmlText(rfNode));
	msg->LinkEndChild(element);

	element = new TiXmlElement("Type");
	element->LinkEndChild(new TiXmlText("TX_RF")); ///not important
	msg->LinkEndChild(element);

	++msgNumber;
	element = new TiXmlElement("MsgNo");
	sprintf(msgNoBuf, "%d", msgNumber);
	element->LinkEndChild(new TiXmlText(msgNoBuf));
	msg->LinkEndChild(element);

	element = new TiXmlElement("APDU"); ///not important
	//04=write_req  1=SOID  7=UDO_ID  req_id  attr_id  size  payload=01(apply)
	//sprintf(apdu, "0318 %02X 04 01 01", ++reqID);
	sprintf(apdu, "031%s %02X 04 01 01", udoObjIDBuf, ++reqID);
	element->LinkEndChild(new TiXmlText(apdu));
	msg->LinkEndChild(element);

	///finished generating
	doc.SaveFile("UDOTest.xml");
	delete [] buf;
}

////////////////////////////////////////////////////////////////////////////////
/// @author sorin.bidian
/// @brief Generate XML element that matches an Execute Response on RX_RF messages.
/// @param msg	Parent element
/// @param processingTimeBuf	Timeout value
/// @remarks This method is used for UDO test - generation of xml messages.
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::generateWaitElementExecResp(TiXmlElement *msg, char *processingTimeBuf, char *udoObjIDBuf) {
	TiXmlElement *element = new TiXmlElement("Wait");
	element->SetAttribute("timeout", processingTimeBuf);
	msg->LinkEndChild(element);
	TiXmlElement *innerElement = 0;
	innerElement = new TiXmlElement("Match");
	innerElement->SetAttribute("type", "RX_RF");
	innerElement->SetAttribute("layer", "APP");
	innerElement->SetAttribute("offset", "0");
	innerElement->SetAttribute("size", "4");
	//innerElement->LinkEndChild(new TiXmlText("8581")); ///match for exec resp = 85, SOID/DOID = 81
	char matchArray[5] = {0};
	sprintf(matchArray, "85%s1", udoObjIDBuf);
	innerElement->LinkEndChild(new TiXmlText(matchArray)); ///match for exec resp = 85, SOID/DOID
	element->LinkEndChild(innerElement);
}

////////////////////////////////////////////////////////////////////////////////
/// @author sorin.bidian
/// @brief Generate XML elements that match the request ID and SFC code on RX_RF messages.
/// @param msg	Parent element
/// @param processingTimeBuf	Timeout value
/// @param reqIDBuf	Expected request ID value
/// @remarks This method is used for UDO test - generation of xml messages.
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::generateWaitElementsIdSfc(TiXmlElement *msg, char *processingTimeBuf, char *reqIDBuf) {
	TiXmlElement *element = new TiXmlElement("Wait");
	element->SetAttribute("timeout", processingTimeBuf);
	msg->LinkEndChild(element);
	TiXmlElement *innerElement = new TiXmlElement("Match");
	innerElement->SetAttribute("type", "RX_RF");
	innerElement->SetAttribute("layer", "APP");
	innerElement->SetAttribute("offset", "4");
	innerElement->SetAttribute("size", "2");
	innerElement->LinkEndChild(new TiXmlText(reqIDBuf));
	element->LinkEndChild(innerElement);

	element = new TiXmlElement("Wait");
	element->SetAttribute("timeout", processingTimeBuf);
	msg->LinkEndChild(element);
	innerElement = new TiXmlElement("Match");
	innerElement->SetAttribute("type", "RX_RF");
	innerElement->SetAttribute("layer", "APP");
	innerElement->SetAttribute("offset", "8");
	innerElement->SetAttribute("size", "2");
	innerElement->LinkEndChild(new TiXmlText("00")); ///match for sfc=success
	element->LinkEndChild(innerElement);
}

////////////////////////////////////////////////////////////////////////////////
/// @author sorin.bidian
/// @brief Generate XML elements that follow the <APDU> element. These elements are common for all UDO requests generated for UDO test.
/// @param msg	Parent element
/// @param ssIpv6		Name of the variable defined in "EXPORT" section of the configuration file, that holds the Ipv6 address of the ScriptServer
/// @param dutUdoIpv6	Name of the variable defined in "EXPORT" section of the configuration file, that holds the Ipv6 address of the DUT
/// @param p_udoPort	Value of the variable defined in "EXPORT" section of the configuration file, that holds the port of UDO object on DUT
/// @param p_securityPolicy	Value of an optional variable defined in "EXPORT" section of the configuration file for the encryption policy (5=enabled, 0=disabled)
void ScriptServer::generateElementsAfterApduUdo(TiXmlElement *msg, const char *ssIpv6, const char *dutUdoIpv6,
		const char *p_udoPort, const char *p_securityPolicy) {
	TiXmlElement *element = 0;
	element = new TiXmlElement("TLEncrypt");
	if (!p_securityPolicy) { //if not defined, use encryption enabled as default
		element->LinkEndChild(new TiXmlText("5")); //"0"= no encryption, "5"= encryption enabled
	} else {
		char secArray[2] = {0};
		sprintf(secArray, "%s", p_securityPolicy);
		element->LinkEndChild(new TiXmlText(secArray));
	}
	msg->LinkEndChild(element);

	element = new TiXmlElement("Priority");
	element->LinkEndChild(new TiXmlText("0"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("DiscardEligible");
	element->LinkEndChild(new TiXmlText("1"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("ECN");
	element->LinkEndChild(new TiXmlText("1"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("IPv6Src");
	//element->SetAttribute("addr", "{SS_IPV6Add_PORT}");
	char ssArray[25] = {0};
	sprintf(ssArray, "{%s}", ssIpv6);
	element->SetAttribute("addr", ssArray);
	element->SetAttribute("port", "61617");
	msg->LinkEndChild(element);

	element = new TiXmlElement("IPv6Dst");
	//element->SetAttribute("addr", "{DUT1_IPV6Add}");
	char dutArray[25] = {0};
	sprintf(dutArray, "{%s}", dutUdoIpv6);
	element->SetAttribute("addr", dutArray);
	//element->SetAttribute("port", "61616");
	char portArray[10] = {0};
	sprintf(portArray, "%s", p_udoPort);
	element->SetAttribute("port", portArray);
	msg->LinkEndChild(element);

	element = new TiXmlElement("ContractID");
	element->LinkEndChild(new TiXmlText("1"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("UDPCompression");
	element->LinkEndChild(new TiXmlText("239"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("NLHdr");
	element->LinkEndChild(new TiXmlText("7D77"));
	msg->LinkEndChild(element);

	element = new TiXmlElement("LinkMsg");
	element->LinkEndChild(new TiXmlText(" ")); ///content not important
	msg->LinkEndChild(element);

	element = new TiXmlElement("DLLHdr");
	element->LinkEndChild(new TiXmlText(" "));
	msg->LinkEndChild(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Process and run messages from XML file
/// @param in	Input stream
/// @retval error code
////////////////////////////////////////////////////////////////////////////////
int ScriptServer::RunScript(std::istream*&in)
{
	char line[5*1024] = { 0 } ;
	struct Params params1 ;
	std::stringstream expandedLine1 ;
	bool retry = false;
	
	parseConfig() ;
	LOG_INFO("-----------------------------------------------------------------\n") ;
	for ( int i = 1;; i++)
	{
		char *outLine(NULL), *rmtLine(NULL) ;
		struct Params params ;
		int rmtMsgType;
		int outLineSz;

		in->getline(line, sizeof(line)) ;
		if ( in->eof() ) return 2 ;

		LOG_INFO( "BeginMessage [%i]\n", i) ;
		LOG_INFO( "\tREAD CSV: [%s]\n", line) ;

		if ( !readParams(params, line, outLine,outLineSz))
		{
			//LOG_INFO("\n RKP: Ln 740\n\n");
			LOG_INFO("\tTest failed\nEndMessage\n\n") ;
			//LOG_INFO("\n RKP: Ln 740\n\n");
			free(outLine);
			free(rmtLine);
			return 3 ;
		}

		if ( !wait(params, rmtLine) )
		{
			if (retry == true)
			{	
					LOG_INFO("\n@@@@@@@@@ RKP:RETRY @@@@@@@@@\n");
					std::string temp = expandedLine1.str().substr(0,expandedLine1.str().length() - 18);
					expandedLine1.str("");
					expandedLine1.str().clear();
					expandedLine1 << temp;
					sendline(params1, expandedLine1 ) ;
					retry = false;
				
			}
			else
			{
				LOG_INFO("\tTest failed\nEndMessage\n\n") ;
				free(outLine);
				free(rmtLine);
				return 3 ;
			}
			
			if ( !wait(params, rmtLine) )
			{
				//LOG_INFO("\n RKP: Ln 740\n\n");
				LOG_INFO("\tTest failed\nEndMessage\n\n") ;
				//LOG_INFO("\n RKP: Ln 740\n\n");
				free(outLine);
				free(rmtLine);
				return 3 ;
			}
		}

		::getMsgType(rmtLine, rmtMsgType/*,false*/);
		if ( !loadAll(params.LoadVec, rmtLine, outLine, rmtMsgType, params.msgType,outLineSz)
		|| ( !saveAll(params.StoreVec, rmtLine, params.msgType) ) )
		{
			//LOG_INFO("\n RKP: Ln 752\n\n");
			LOG_INFO("\tTest failed\nEndMessage\n\n") ;
			//LOG_INFO("\n RKP: Ln 752\n\n");
			free(outLine);
			free(rmtLine);
			return 4 ;
		}

		if ( outLine && *outLine && !(params.policy&POLICY_NOSEND) )
		{
			if ( !params.loop.increment ) { params.loop.start=0; params.loop.end=1; }
			for ( g_oCfg.loopIdx = params.loop.start
			    ; g_oCfg.loopIdx < params.loop.end
			    ; g_oCfg.loopIdx+= params.loop.increment
			    )
			{
				std::stringstream expandedLine ;
				expandPlaceHolders(outLine, expandedLine ) ;
				sendline(params, expandedLine ) ;
				params1 = params;
				expandedLine1.str("");
				expandedLine1.str().clear();
				expandedLine1 << expandedLine.rdbuf();
				retry = true;
				if ( params.loop.end-params.loop.start > 1 ) sleep(2) ;
			}
		}
		free(outLine);
		free(rmtLine);
		LOG_INFO("EndMessage [%i]\n\n", i) ;
	}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Wait an incoming message from BBR and perform a match on it.
/// @param params	Message parameters extracted from XML
/// @param inLine	Message received form BBR
/// @retval false when matching was unsuccessful
/// @see match
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::wait(Params& params, char*& inLine)
{
	if ( params.policy&POLICY_NORECV )
		return true ;

	int s, rv, timeout(params.timeout) ;
	/**/struct sockaddr_in servaddr, cliaddr ;

	if ( !timeout ) timeout = g_oCfg.DefaultTimeout ;

	/**/if ( (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 )
		diep("socket") ;

	inLine = NULL ;
	struct timeval tv = { timeout, 0 } ;

try_again:
	bzero(&servaddr, sizeof(servaddr)) ;
	servaddr.sin_family = AF_INET ;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY) ;
	servaddr.sin_port = htons(params.ackLoggerPort) ;
	bind(s, (struct sockaddr *) &servaddr, sizeof(servaddr)) ;
        /**/
	LOG_INFO( "\tWAITING : [host:0.0.0.0] [port:%i] [seconds:%i]\n", params.ackLoggerPort, timeout ) ;
	//LOG_INFO("\n RKP: Ln:811\n")
	fd_set rfds ;
	FD_ZERO(&rfds) ;
	FD_SET(s,&rfds) ;
	rv = select(s + 1, &rfds, 0, 0, &tv) ;
	//printf("\n rv %i \n",rv);
	//LOG_INFO("\n RKP: Ln:816\n")
	char mesg[65535] ;
	if ( rv == -1 )
	{
		LOG_INFO( "Error: Select failed\n") ;
		close(s) ;
		return false ;
	} else if ( rv )
	{
		memset(mesg, 0, sizeof(mesg) ) ;
		socklen_t len = sizeof(cliaddr) ;
		int n = recvfrom(s, mesg, sizeof(mesg) , 0,
				(struct sockaddr *) &cliaddr, &len) ;
		mesg[n] = 0 ;
		if ( mesg[n - 1] == '\n' )
			mesg[n - 1] = 0 ;
		char *srcHost = inet_ntoa(cliaddr.sin_addr) ;
		LOG_INFO( "\tREAD UDP [%s]: [host:%s] [port:%i] [%s]\n", szNow(), srcHost, params.ackLoggerPort, mesg) ;
		//if ( strcmp(srcHost, params.host) )
		//{
		//	LOG_INFO("Error: Test Failed: Received a packet from IP[%s] other than expected[%s]\n", srcHost, params.host ) ;
		//	close(s) ;
		//	return false ;
		//}
		if ( params.timeout )
		{
			updateTAIDesync(mesg); ///keep the TAI desync list updated

			LOG_INFO( "\tMATCHING: ") ;
			std::vector<struct Tagwait>::iterator it = params.WaitVec.begin() ;
			for ( ; it != params.WaitVec.end(); ++it)
			{
				MATCH_TYPE mt = match(mesg, *it, params.policy) ;
				if ( MATCH_DROP == mt )
				{
					goto try_again ;
				} else if ( MATCH_FAILED == mt )
				{
					close(s) ;
					return false ;
				} else if ( MATCH_OK == mt )
				{
					continue ;
				}
			}
		}
	} else
	{
		//LOG_INFO("\n RKP: Ln:865 Entering Elseb block")
	if (params.policy&POLICY_WAIT)
	{
		close(s) ;
		return true ;
	}
else if(params.policy&POLICY_FAILPASS)
{
		LOG_INFO( "Response not received in %d seconds as expected\n", timeout ) ;
		close(s) ;
		return true ;

}
else if(params.policy&POLICY_FAILCONTINUE)
{
		//LOG_INFO( "\n RKP: Ln:877 \n") ;
		printf("Params.policy %i",params.policy);
		LOG_INFO( "No Response in %d seconds \n TEST FAILED\n\n", timeout ) ;
		//LOG_INFO( "\n RKP: Ln:877 \n") ;
		close(s) ;
		return true ;

}
else
{
		//LOG_INFO( "\n RKP: Ln:882 \n") ;
		//printf("Params.policy %i",params.policy);
		LOG_INFO( "Error: No response in %d seconds\n", timeout ) ;
		//LOG_INFO( "\n RKP: Ln:882 \n") ;
		close(s) ;
		return false ;
}
	}
	close(s) ;
	inLine = strdup(mesg) ;
	return true ;
}


#if 0
int ScriptServer::wait(Params& params, char*& inLine)
{
		//LOG_INFO( "\n RKP: Ln:905 In Wait2 \n") ;
	if ( params.policy&POLICY_NORECV )
	{
		return true ;
	}
	inLine = NULL ;
	int s, slen, rv, timeout(params.timeout) ;
	udp( params.host, params.ackLoggerPort);
	udp.bind(params.ackLoggerPort);
	poller.add(udp);
	LOG_INFO( "\tWAITING : [host:0.0.0.0] [port:%i] [seconds:%i]\n", params.ackLoggerPort, timeout ) ;
	if ( ! poller.isset(udp) )
	{
		LOG_INFO( "Error: No response in %d seconds\n", timeout ) ;
		return false;
	}
	udp.read();
	LOG_INFO( "\tREAD UDP [%s]: [host:%s] [port:%i] [%s]\n", szNow(), srcHost, params.ackLoggerPort, mesg) ;
	if ( params.timeout )
	{
		LOG_INFO( "\tMATCHING: ") ;
		std::vector<struct Tagwait>::iterator it = params.WaitVec.begin() ;
		for ( ; it != params.WaitVec.end(); ++it)
		{
			MATCH_TYPE mt = match(mesg, *it, params.policy) ;
			if ( MATCH_DROP == mt )
			{
				goto try_again ;
			} else if ( MATCH_FAILED == mt )
			{
				close(s) ;
				return false ;
			} else if ( MATCH_OK == mt )
			{
				continue ;
			}
		}
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Store the desynchronization value between TAI of a RX_RF message and current TAI of the server
/// @param mesg		Received message
/// @remarks Values are stored in a list and an average is computed when the TAI_Offset placeholder is called. The list will always contain the latest 3 values.
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::updateTAIDesync(char * mesg) {
	int type;
	::getMsgType(mesg,type);
	if (type == RX_RF) {
		char *p_tai(mesg);
		///jump over commas to get TAI value
		bool err = false;
		for (int i = 0; i < 8; i++) { //TAI field is after 8 commas
			if ( ! (p_tai = strchr(p_tai,',')) )
			{
				LOG_INFO("Error encountered while getting TAI value from message; (comma=%d)\n", i);
				err = true;
				break;
			}
			if ( *p_tai == ',' ) p_tai++ ;
		}
		if (!err) {
			long taiVal = 0;
			sscanf(p_tai, "%ld", &taiVal);
			if (taiVal) {
				int desync = taiVal - time(NULL) - (0x16925E80 + 34); ///desync between message TAI and current TAI
				LOG_INFO("\tTAI desync=%d\n", desync);
				taiDesyncQ.push_back(desync);
				if (taiDesyncQ.size() > 3) { ///limit storing to 3 values
					taiDesyncQ.pop_front();
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Perform matching on message data, using Wait/Match parameters extracted from XML
/// @param p	Message data
/// @param w	Wait/Match parameters
/// @param policy	Message policy
/// @retval	error code
/// @remarks Used to match by checking for equal
////////////////////////////////////////////////////////////////////////////////
MATCH_TYPE ScriptServer::matchLiteral(const char*p, struct Tagwait&w, int policy)
{
char *binary=(char*)malloc(10*sizeof(char));//For storing half byte when bitchk is 1
if(w.bitchk!=0)//if bit chk is enabled convert half byte to binary
{
//LOG_INFO( "BITCHECK enabled\n") ;
//LOG_INFO( "Enabled bch\n") ;
if(w.size!=1)
{
LOG_INFO( "Error: Bit compare only for HALF byte\n") ;
return MATCH_FAILED ;
}
else
{
//LOG_INFO("\n p=%s",p+w.offset+w.size);
binary=getBinary(p[w.offset]); // Convert the digit to binary
binary[4]=0;
w.size=4;
}
}
	if ( (w.bitchk!=0)?memcmp(binary, w.data, w.size):memcmp(p + w.offset, w.data, w.size) )//If bitchk enabled compare with binary value
	{
    if(w.bitchk!=0)
       {
 LOG_INFO( "Error: Rsp_wait(l:%u)(o:%i) failed->expecting(%s) != received(%s)\n", w.layer, w.offset, w.data, binary) ;
       }
    else
       {
 LOG_INFO( "Error: Rsp_wait(l:%u)(o:%i) failed->expecting(%s) != received(%s)\n", w.layer, w.offset, w.data, p+w.offset) ;
       }
		if ( policy & POLICY_NOMATCH_DROP )
		{
			LOG_INFO( "Rsp_wait dropped\n") ;
			return MATCH_DROP ;
		}
	    else if( ( policy & POLICY_WAIT )||( policy & POLICY_FAILCONTINUE )||( policy & POLICY_FAILPASS ))
		{
			LOG_INFO( "Rsp_wait dropped\n") ;
			return MATCH_DROP ;
		}

	return MATCH_FAILED ;
	}








if(w.typechk==0)//When we dont need to check type constraint
{
if ( policy & POLICY_FAILPASS )
{
	LOG_INFO( "\tWAIT NOT ok(l:%u)(o:%i) [%s] == [%s]\n", w.layer, w.offset, w.data, p+w.offset) ;
	return MATCH_FAILED ;
}
else
{
		if(w.bitchk!=0)
{
	LOG_INFO( "\tWAIT ok(l:%u)(o:%i) [%s] == [%s]\n", w.layer, w.offset, w.data,binary) ;
}
else
{
	LOG_INFO( "\tWAIT ok(l:%u)(o:%i) [%s] == [%s]\n", w.layer, w.offset, w.data, p+w.offset) ;

}
	return MATCH_OK ;

}
}
else
{

if(memcmp(p+w.offset+w.size,",",1))//Check if the character after expected size is a Comma
{
LOG_INFO( "\t\nERROR:VALUE RECEIVED BEYOND EXPECTED SIZE: %i\n", w.size) ;

		if ( policy & POLICY_NOMATCH_DROP )//Repeat same policy checks for match drop and failed case
		{
			LOG_INFO( "Rsp_wait dropped\n") ;
			return MATCH_DROP ;
		}
	    else if( ( policy & POLICY_WAIT )||( policy & POLICY_FAILCONTINUE )||( policy & POLICY_FAILPASS ))
		{
			LOG_INFO( "Rsp_wait dropped\n") ;
			return MATCH_DROP ;
		}

	return MATCH_FAILED ;

}
else// There is no character beyond expected size
{
if ( policy & POLICY_FAILPASS )
{
	LOG_INFO( "\tWAIT NOT ok(l:%u)(o:%i) [%s] == [%s]\n", w.layer, w.offset, w.data, p+w.offset) ;
	return MATCH_FAILED ;
}
else
{
	LOG_INFO( "\tWAIT ok(l:%u)(o:%i) [%s] == [%s]\n", w.layer, w.offset, w.data, p+w.offset) ;
	return MATCH_OK ;

}
}
}
}//}}}

////////////////////////////////////////////////////////////////////////////////
/// @brief Perform matching on message data, using Wait/Match parameters extracted from XML.
/// @param p	Message data
/// @param w	Wait/Match parameters
/// @param policy	Message policy
/// @retval	error code
/// @remarks Used to compare native types
////////////////////////////////////////////////////////////////////////////////
MATCH_TYPE ScriptServer::matchNativeType(const char *p, Tagwait& w, int policy)
{
	if ( w.size > 4 ) /* we cant handle native types larger than 4 bytes */
	{
		LOG_INFO("Error: Unable to compare data larger than 4 bytes") ;
		return MATCH_FAILED ;
	}
	unsigned long oprd1(0), oprd2(0) ;
	char buf[8];

	memcpy(buf, p + w.offset, w.size) ;
	buf[w.size]=0;
	sscanf( buf, "%lx", &oprd1 ) ;

	memcpy( buf, w.data, w.size) ;
	buf[w.size]=0;
	sscanf( buf, "%lx", &oprd2 ) ;

	if ( (w.op == OP_LT && oprd1 < oprd2)
	||   (w.op == OP_LE && oprd1  <= oprd2)
	||   (w.op == OP_GT && oprd1 > oprd2)
	||   (w.op == OP_GE && oprd1 >= oprd2) )
	{
	LOG_INFO("compare OK: %lx %lx\n", oprd1, oprd2);
		return MATCH_OK ;
	}
	LOG_INFO("compare failed: %lx %lx\n", oprd1, oprd2);
	return MATCH_FAILED ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Perform matching on message received from BBR, using Wait/Match parameters extracted from XML
/// @param mesg		Received message
/// @param params	Wait/Match parameters
/// @param policy	Message policy
/// @retval	error code
////////////////////////////////////////////////////////////////////////////////
MATCH_TYPE ScriptServer::match(char * mesg, struct Tagwait& cmp, int policy)
{
	int nbCommas = offset( MsgLayout[cmp.msgType], (FIELD_TYPE)cmp.layer ) ;
	if ( -1 == nbCommas )
	{
		LOG_ERROR( "Unknown layer/msg:%i \n", cmp.layer );
		return MATCH_FAILED ;
	}
	char *p(mesg) ;
	/* preconditions */
	if ( cmp.op == OP_UNKNOWN ) return MATCH_FAILED ;

	int type;
	::getMsgType(mesg,type);

	/* Verify the expected message type. */
	if ( cmp.msgType != type )
	{
		if (policy&POLICY_NOMATCH_DROP)
		{
			return MATCH_DROP;
		}

	else if (policy&POLICY_WAIT)
		{
			return MATCH_DROP;
		}
	else if (policy&POLICY_FAILPASS)
		{
			return MATCH_DROP;
		}
       else if (policy&POLICY_FAILCONTINUE)
		{
			return MATCH_DROP;
		}
		else
		{
			LOG_INFO("Error: Unexpected message type. I was waiting for[%s]\n", getMsgType(cmp.msgType) );
			return MATCH_FAILED ;
		}
	}

	/* Jump over commas until the selected layer is found. */
	for ( int i = 0; i < nbCommas; ++i)
	{
		if ( ! (p=strchr(p,',')) )
		{
			LOG_INFO( "Error: Malformed CSV line[%s][%i][%i]\n",mesg, nbCommas, cmp.layer) ;
			return MATCH_FAILED ;
		}
		if ( *p == ',' ) p++ ;
	}

	std::stringstream expandedLine ;
	expandPlaceHolders(p, expandedLine ) ;
	/* compare */
	if ( cmp.op == OP_EQ ) { return matchLiteral(expandedLine.str().c_str(),cmp,policy) ; }
	LOG_INFO("native\n");
	return matchNativeType(expandedLine.str().c_str(),cmp,policy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get message parameters specified in XML
/// @param params	Parameters extracted from message
/// @param line		Message string
/// @param outLine	Remaining string after parameters reading
/// @param outLineSz	Remaining string size
/// @retval	true when parameters are sucessfully read
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::readParams(struct Params& params, char *line, char *& outLine, int& outLineSz)
{
	if ( NULL==line || 0==line[0] ) return false;

	std::string op ; //To get value each time
	CCsv c1, c3 ;

	std::stringstream out ;

	c1.SetLine(line).SetSeparator(':',',') ;
	c1.Get(op) ; // rfNode
int i=0;
	while(op[i]!='\0') //Change to Upper Case
	{
		if(op[i]>=97) op[i]-=32;
		i++;
	}
	params.desc=(char*)op.c_str();
	int t=strlen(params.desc);
	strcpy(desc1,params.desc);
	desc1[t]='\0';

	//unsigned char ch[10];

	LOG_INFO("\n-------MESSAGE  DESCREPTION: %s--------- \n\n", params.desc);

	c1.Get(op) ; // rfNode

	if ( !getRfNode(op.c_str(), params.host, params.ackLoggerPort, params.backbonePort) )
		return false ;

	/* Read message type */
	c1.Get(op).Get(params.timeout) ;
	LOG_INFO("\tOPTIONS : [host:%s] [timeout:%d] \n", params.host, params.timeout) ;
//LOG_INFO("\nparams.msgType:%s\n",op.c_str());
	::getMsgType(op.c_str(), params.msgType) ;
//LOG_INFO("After calling1\n");

	/* Read policy */
	c1.Get(op) ;
	getPolicyType(op.c_str(), params.policy) ;

	/* Read loop */
	c1.Get(op) ;
	getLoop(op.c_str(), params.loop);

	/* Read Wait/Match */
	std::string waits ;
	while ( !c1.Eor() )
	{
		c1.Get(waits) ;
		if ( waits == "" )
			break ;

		c3.SetLine(waits.c_str()).SetSeparator('|') ;

		struct Tagwait w ;
		/* Read message operator*/
		getOpType( c3, w.op) ;
              c3.Get(w.typechk);// Get type check value
              c3.Get(w.bitchk);// Get bit check value
	       c3.Get(w.reversechk);// Get bit check value

		c3.Get(w.id) ;//Get id to compare saved Data
		if ( *w.id == '\0' )
		{
			free(w.id) ;
			w.id = NULL ;
		}
		//LOG_INFO("\nTypech=%i Bitchk=%i id=%s\n",w.typechk,w.bitchk,w.id);
		getLayerType( c3, w.src.layer) ; //For stored id
		c3.Get(w.src.offset) ; //For stored id
              c3.Get(w.srcSize); // For stored id
		
		/* Read message type */
		::getMsgType( c3, w.msgType) ;
		//LOG_INFO("After calling 2\n");

		/* Read layer name */
		getLayerType( c3, w.layer) ;

		/* Read offset, size and data */
		char* data;
		
		char* size_char;
		char* offset_char;
			
		/*Logic is modified by HONEYWELL *//* RK PRAVEEN*/
		c3.Get(offset_char).Get(size_char).Get(data) ;
		
		int i =0,j=1,k=0;
		char val_ch[5];
		char Tmp[30];
		int OperatorFlag = 0;
		int Cnst = 0;
		Tmp[0]='{';
		
		if(offset_char[0] != '{')
		{
			
			w.offset = atoi(offset_char);
			
		}
		
		else
		{
		
			while(offset_char[i] != '}')
			{
				if((offset_char[i]>= 48 && offset_char[i] < 58) && (OperatorFlag == 0))
				{
					val_ch[k]= offset_char[i];
					k++;
				}
				else if(offset_char[i]>= 65 && offset_char[i]<=122)
				{
					Tmp[j] = offset_char[i];
					j++;
				}
				else if(offset_char[i] == '+')
				{
					OperatorFlag = 1;
					val_ch[k] = '\0';
				}	
				i++;
			}
			
			
			Tmp[j]='}';
			
			Tmp[++j]='\0';
			
			offset_char = Tmp;
			
			
			
			if(OperatorFlag == 1)
			{
				
				Cnst = atoi(val_ch);
				
			}
			
			std::stringstream os_offset ;
			expandPlaceHolders(offset_char,os_offset);
			
			w.offset_char = strdup( os_offset.str().c_str() );
			int offset_tmp = atoi(w.offset_char);
			
			w.offset = offset_tmp;
			
			if(OperatorFlag == 1)
			{
				w.offset += Cnst;
			}
			
			
		
		
		}
		
	
		i = 0;j=1;k=0;
		char Tmp1[30];
		OperatorFlag = 0;
		Tmp1[0]='{';
		
		if(size_char[0] != '{')
		{
			
			w.size = atoi(size_char);
			
		}
		else
		{
		
			while(size_char[i] != '}')
			{
				if((size_char[i]>= 48 && size_char[i] < 58) && (OperatorFlag == 0))
				{
					val_ch[k]= size_char[i];
					k++;
				}
				else if(size_char[i]>= 65 && size_char[i]<=122)
				{
					Tmp1[j] = size_char[i];
					j++;
				}
				else if(size_char[i] == '+')
				{
					OperatorFlag = 1;
					val_ch[k] = '\0';
				}	
				i++;
			}

			
			Tmp1[j]='}';
				
			Tmp1[++j]='\0';
					
			size_char = Tmp1;
			
			Cnst = 0;
			
			if(OperatorFlag == 1)
			{
				
				Cnst = atoi(val_ch);
				
			}
			
			std::stringstream os_size ;
			expandPlaceHolders(size_char,os_size);
		
			w.size_char = strdup( os_size.str().c_str() );
			int size_tmp = atoi(w.size_char);
			
			w.size = size_tmp;
		
			if(OperatorFlag == 1)
			{
				w.size += Cnst;
			}
			
			
			
		}
		
		
		
if(w.id)//Get stored data to compare
{
if(!getStoredCompare(w,data))
{
	LOG_DEBUG("Error:Unable to get stored id:%s\n", w.id ) ;
       return false;
}

}

		std::stringstream os ;
		expandPlaceHolders(data,os);
		w.data = strdup( os.str().c_str() );
		

if(w.reversechk)
{

int i1=0,i2=strlen(w.data)-2;
char t1,t2;
while(i1<i2)
{
t1=w.data[i1];
t2=w.data[i1+1];
w.data[i1]=w.data[i2];
w.data[i2]=t1;
w.data[i1+1]=w.data[i2+1];
w.data[i2+1]=t2;
i2-=2;
i1+=2;

}

}
		params.WaitVec.push_back(w) ;
	}

	c1.Eor(false) ;
	/* Read Modify/Save */
	std::string save ;

	while ( ! c1.Eor() )
	{
		c1.Get(save) ;
		if ( save == "" )
		{
			
			LOG_DEBUG("No Modify/Save\n");
			break ;
		}

		c3.SetLine(save.c_str()).SetSeparator('|') ;

		struct TagModify m ;
		c3.Get(m.id) ;
		if ( *m.id == '\0' )
		{
			free(m.id) ;
			m.id = NULL ;
		}
		c3.Get(m.operation) ;
		c3.Get(m.size) ;
		getLayerType( c3, m.src.layer) ;
		c3.Get(m.src.offset) ;
		//LOG_INFO("\t\t ***RKP LINE:1356 Save:[id:%s],[size:%u],[layer:%s],[offset:%i]***  \n", m.id, m.size, op.c_str() ,m.src.offset ) ;
		params.StoreVec.push_back( m );
		//LOG_INFO("\n Ln:1500 End of save\n");
	}

	/* Read Modify/Copy */
	std::string copy ;
	while ( ! c1.Eor() )
	{
		c1.Get(copy) ;
		if ( copy == "" )
			break ;

		c3.SetLine(copy.c_str()).SetSeparator('|') ;

		struct TagModify m ;
		c3.Get(m.id) ;
		if ( *m.id == '\0' )
		{
			free(m.id) ;
			m.id = NULL ;
		}
		c3.Get(m.size) ;
		getLayerType( c3, m.src.layer) ;
		c3.Get(m.src.offset) ;
		LOG_DEBUG("Copy:Src[id:%s],[size:%u],[layer:%i],[offset:%i]  ", m.id, m.size, m.src.layer ,m.src.offset ) ;

		/* read dst layer type */
		getLayerType( c3, m.dst.layer) ;
		c3.Get(m.dst.offset) ;
		LOG_DEBUG("Copy:Dst[layer:%i],[offset:%i]\n", m.dst.layer,m.dst.offset ) ;
		params.LoadVec.push_back(m) ;
	}
	
	if ( *c1.CurrentIt() == ',' )
	{
		size_t s =strlen( (char*) (c1.CurrentIt()+1) );
		outLineSz =s;
		outLine = (char*)malloc( outLineSz+1 );
		memcpy(outLine, (char*) (c1.CurrentIt()+1), s );
		outLine[s]=0;
	}
	else
	{
		size_t s =strlen ( (char*) (c1.CurrentIt()) );
		outLineSz = s ;
		outLine = (char*)malloc( outLineSz );
		memcpy(outLine, (char*) (c1.CurrentIt()), s );
		outLine[s]=0;
	}

	LOG_DEBUG("\n");
	if ( NULL!=params.currentId && *params.currentId == '\0' )
	{
		free(params.currentId) ;
		params.currentId = NULL ;
	}
	
	
	return true ;
}

bool ScriptServer::getLoop( const char*loopStr,struct loop_spec& loop)
{
	loop.increment = 0;
	std::stringstream out;
	expandPlaceHolders(loopStr,out) ;
	int rv = sscanf(out.str().c_str(),"%x;%x;%x", &loop.start, &loop.end, &loop.increment);
	if ( !loop.increment )
	{
		loop.increment = 1;
		if ( rv <2 )
		{
			loop.start=0;
			loop.end=1;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Apply the modifications specified in XML Modify/Copy elements
/// @param m	Modifications list
/// @param src	Message string
/// @param dst	Modified string
/// @param inType	Received message type
/// @param myType	Message type saved from XML message
/// @param dstSize	Modified string size
/// @retval true when modification were applied successfully
/// @remarks When data is copied from source string to destination string, if ',' delimiter is reached (in source) before copying the specified length,
/// copy will stop at that delimiter.
/// When the destination string size is too short, the string is resized so the data from source can fit in.
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::loadAll(std::vector<struct TagModify>& m, char* src, char*& dst, int inType, int myType, int& dstSize)
{
	for ( size_t i = 0; i < m.size(); ++i)
	{
		if ( m[i].id )
		{
			if ( g_oCfg.StorageMap.find( m[i].id ) == g_oCfg.StorageMap.end() )
			{
				LOG_INFO("ERROR: undefined reference to ID:%s. It was not saved previously\n", m[i].id ) ;
				//Modified By Honeywell - RK Praveen
				//return false to true - change is made to continue for failcontinue to pass though buffer is empty
				return true ;
			}
			src = g_oCfg.StorageMap[m[i].id] ; // check to see if the id is in the map
			LOG_INFO("\tLoad saved content:[id:%s]<%s>\n", m[i].id, src) ;
		}

		/*Position */
		int srcCommas, dstCommas ;
		srcCommas = offset(MsgLayout[inType], FIELD_TYPE(m[i].src.layer) );
		dstCommas = offset(MsgLayout[myType], FIELD_TYPE(m[i].dst.layer) );
		LOG_DEBUG("\tinType:%i myType:%i srcCommas:%i dstCommas:%i\n", inType, myType, srcCommas, dstCommas);
		if ( srcCommas==-1 || dstCommas==-1 )
		{
			LOG_ERROR("ERROR: Unable to find srcLayer or dstLayer\n");
			return false;
		}

		char *psrc = searchComma( src, srcCommas);
		if ( NULL==psrc ) return false;
		char *pdst = searchComma( dst, dstCommas);
		if ( NULL==pdst ) return false;

		int copyLength = m[i].size; ///number of characters to copy

		///copy m[i].size characters from source, but stop if ',' is reached before copy length
		char *pSrcLimit = searchComma(psrc + m[i].src.offset, 1); ///search first ',' from copy start position
		if (pSrcLimit) {
			copyLength = pSrcLimit - (psrc + m[i].src.offset) - 1;
		}
		if (copyLength > m[i].size) {
			copyLength = m[i].size;
		}
		else if (copyLength < m[i].size) {
			LOG_INFO("\tPreparing copy - encountered ',' delimiter -> limit copy size to %i (instead of %i)\n", copyLength, m[i].size);
		}

		///resize dst when there's more to copy than it fits
		int dstCopySpace = dstSize - (pdst - dst) - m[i].dst.offset;
		char *pDstLimit = searchComma(pdst + m[i].dst.offset, 1); ///search first ',' from copy start position
		if (pDstLimit) {
			dstCopySpace = pDstLimit - (pdst + m[i].dst.offset) - 1;
		}

		if (dstCopySpace < 0) {
			LOG_ERROR("ERROR: invalid offset specified for COPY>Dst field: %i\n", m[i].dst.offset);
			return false;
		}

		if (dstCopySpace < copyLength) {
			LOG_INFO("\tDestination copy space is not enough -> resize (size=%i, needed=%i) \n", dstCopySpace, copyLength);
			int extraSpaceNeeded = copyLength - dstCopySpace;
			dstSize += extraSpaceNeeded + 1; ///+1 for string terminator
			char *tempDst = (char*) realloc(dst, dstSize);
			if (tempDst) {
				dst = tempDst;
			}
			else {
				LOG_ERROR("ERROR: Unable to reallocate memory for COPY; size=%u\n", dstSize);
				return false;
			}

			*(dst + dstSize - 1) = 0;

			pdst = searchComma(dst, dstCommas); ///reposition pointer to copy location
			if ( NULL==pdst ) return false ;

			///reposition data that followed copy location - needed only if there was data after destination copy space
			char *pDstLimit = searchComma(pdst + m[i].dst.offset, 1); ///search first ',' from copy start position
			if (pDstLimit) {
				int moveLength = dstSize - extraSpaceNeeded - (pDstLimit - dst) + 1; /// +1 = includes delimiter
				LOG_DEBUG("\t reposition data - move %i chars\n", moveLength);
				memmove(pDstLimit - 1 + extraSpaceNeeded, pDstLimit - 1, moveLength); /// -1 = includes delimiter
			}
		}

		memcpy(pdst + m[i].dst.offset, psrc + m[i].src.offset, copyLength) ;

		LOG_INFO("\tCOPY(sz:%i srcOffset:%i dstOffset:%i srcComma:%i dstComma:%i):<",
				copyLength, m[i].src.offset, m[i].dst.offset, srcCommas, dstCommas) ;
		for ( int o = 0; o < copyLength; ++o)
		{
			LOG_INFO("%c", *(pdst+m[i].dst.offset+o) ) ;
		}
		LOG_INFO(">\n") ;
	}
	return true ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Apply the modifications specified in XML Modify/Save elements
/// @param m	Modifications list
/// @param src	Message string
/// @param myType	Message type saved from XML message
/// @retval true when the modifications were applied successfully
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::saveAll(std::vector<struct TagModify>& m, char* src, int type)
{
	//LOG_INFO("RKP: saveAll Start");
	for ( size_t i = 0; i < m.size(); ++i)
	{
		//LOG_INFO("\nRKP: In For Ln: 1700");
		int srcCommas, dstCommas ;
		//if ( ! m[i].id ) continue ;

		srcCommas=offset(MsgLayout[type], FIELD_TYPE(m[i].src.layer) );
		char*psrc = searchComma( src, srcCommas ) ;
		//if ( NULL==psrc ) return false ;
		//Modified by honeywell - RK Praveen
		if ( NULL==psrc ) return true ;
		//LOG_INFO("\nRKP: Ln: 1707\n");
		char * content ;
		if ( m[i].size != 0 )
		{
			content = (char*) calloc(m[i].size +1, sizeof(char) );
			memcpy(content, psrc + m[i].src.offset, m[i].size) ;
		}
		else
		{
			content = strdup(src);
		}
		if ( g_oCfg.StorageMap.find( m[i].id ) == g_oCfg.StorageMap.end() )
		{
if(strlen(m[i].operation)!=0)
{
	//LOG_INFO("RKP: Ln: 1722");
long int opData,newvalue,iddata;
long int tcontent; //temp
char operator1;
	//char* src,*OrigData=data;
       sscanf( m[i].operation, "%c%lx",&operator1,&opData );
       sscanf( content, "%lx",&tcontent );
switch(operator1)
{
		//LOG_INFO("RKP: Ln: 1731 in switch");
case '+':
newvalue=opData+tcontent;
break;
case '-':
newvalue=tcontent-opData;
break;
case '*':
newvalue=tcontent*opData;
break;
case '/':
newvalue=tcontent/opData;
if(tcontent%opData!=0) newvalue=newvalue+1;
break;
default:
LOG_INFO("Error:Not a valid operator%c ", operator1) ;
break;
}
//itoa(newvalue,data);
//sscanf(newvalue, "%s",data);
//LOG_INFO("\nRKP: Ln: 1751\n");
sprintf(content, "%lx", newvalue);
int kk=0;
	while(content[kk]!='\0')
	{
		if(content[kk]>=97) content[kk]-=32;
		kk++;
	}
}
			g_oCfg.StorageMap[ m[i].id ] = content;
			LOG_INFO("\tSave content:[id:%s]<%s>\n", m[i].id, g_oCfg.StorageMap[ m[i].id ]) ;
		}
	}
	//LOG_INFO("\nRKP: Ln: 17065 End of Saveall");
	return true ;
	
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the comparison operator from a message
/// @param c	Csv object that contains the message
/// @param type	Type read from message
/// @retval false when operator type could not be determined
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::getOpType(CCsv& c, int& type)
{
	std::string op;
	c.Get(op) ;
	const char*msg=op.c_str();
	for ( unsigned i = 0; g_OperatorTypes[i].tokenName; ++i)
		if ( !strncmp(msg, g_OperatorTypes[i].tokenName, g_OperatorTypes[i].tokenLength) )
		{
			type = g_OperatorTypes[i].type ;
			return true ;
		}
	if ( 0!=msg[0] ) LOG_ERROR( "[operator unknown:%s]",msg) ;
	type = OP_UNKNOWN ;
	return false ;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the name of a message type
/// @param type	Type of the message
/// @retval string representing the name specified by type; empty string is returned when type is unknown
////////////////////////////////////////////////////////////////////////////////
const char* ScriptServer::getMsgType(int type)
{
	for ( unsigned i = 0; g_MsgTypes[i].tokenName; ++i)
		if ( type == g_MsgTypes[i].type )
		{
			return g_MsgTypes[i].tokenName ;
		}

	LOG_INFO( "[msgtype unknown:%i] ",type) ;
	return "" ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the layer type from a message
/// @param c	Csv object that contains the message
/// @param type	Type read from message
/// @retval false when layer type could not be determined
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::getLayerType( CCsv& c, int& type)
{
	std::string op;
	c.Get(op);
	const char*layer=op.c_str();
	if ( !layer ) return FIELD_UNKNOWN ;

	for ( unsigned i = 0; g_LayerTypes[i].tokenName; ++i)
		if ( !strncmp(layer, g_LayerTypes[i].tokenName, g_LayerTypes[i].tokenLength) )
		{
			type = g_LayerTypes[i].type ;
			return true ;
		}
	if ( 0!=layer[0] ) LOG_DEBUG("[layer unknown:%s] ", layer)
	type = FIELD_UNKNOWN ;
	return false ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get policy type out of policy string specified in the XML message
/// @param policy	Policy string
/// @param type		Type read from string
/// @retval false when policy type could not be determined
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::getPolicyType(const char *policy, int& type)
{
	if ( !policy ) return POLICY_UNKNOWN ;

	type=0;
	bool found=false;
	while ( policy && *policy )
	{
	/* The logic has been changed by Honeywell ///Kiran J */
	for (unsigned i=0;i<=7; ++i)
		{
if(i==5) continue;
			if ( !strncmp(policy, g_PolicyTypes[i].tokenName, g_PolicyTypes[i].tokenLength ) )
			{
				type |= g_PolicyTypes[i].type ;
				found=true;
				policy+=strlen(g_PolicyTypes[i].tokenName);
				if ( '|'==*policy ) ++policy ;
			}
		}
	}
	if ( found ) return true ;

	if ( 0 != policy[0] ) LOG_ERROR("[policy unknown:%s] ",policy) ;
	type = POLICY_UNKNOWN ;
	return false ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get RFNode parameters out of RFNode string specified in the XML message
/// @param rfNode	RFNode string
/// @param host		host parameter read from string
/// @param ackLoggerPort	ackLoggerPort parameter read from string
/// @param backbonePort		backbonePort parameter read from string
/// @retval false when the RFNode was not specified in the config file
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::getRfNode(const char *rfNode, char *host, int& ackLoggerPort, int& backbonePort)
{	if (	rfNode[0] == '\0' ||
		RfNodeMap.find(rfNode) == RfNodeMap.end() )
	{
		LOG_ERROR("Error - RF_node[%s] not specified\n", rfNode) ;
		return false ;
	}

	RfNode rfnode = RfNodeMap[rfNode];

	strcpy(host,rfnode.host);
	ackLoggerPort=rfnode.ackLoggerPort ;
	backbonePort =rfnode.backbonePort;
	return true ;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Parse the configuration file (ss.ini) - read the RFNodes parameters
/// and the specified variable values that will be substituted when XML messages are processed
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::parseConfig( )
{
	CSimpleIniA ini(false, true, true) ;
	SI_Error rv = ini.LoadFile("../../Config/ss.ini") ;
	if ( rv != 0 )
	{
		LOG_ERROR("No config file[ss.ini]:%i\n", rv) ;
		return false ;
	}
	CSimpleIniA::TNamesDepend keys ;
	CSimpleIniA::TNamesDepend nodes ;

	ini.GetAllKeys("EXPORT", keys) ;
	CSimpleIniA::TNamesDepend::iterator it = keys.begin() ;
	for ( ; it != keys.end(); ++it)
	{
		LOG_INFO("export %s\n", it->pItem ) ;
		placeholderCallbacks.insert(std::pair<const char*, func_ptr>( strdup(it->pItem), &ScriptServer::GetConfig) ) ;
		//placeholderCallbacks.insert(std::pair<const char*, func_ptr>( it->pItem, &ScriptServer::GetConfig) ) ;
	}

	ini.GetAllKeys("RF_NODES", nodes) ;
	it = nodes.begin() ;
	for ( ; it != nodes.end(); ++it)
	{
		RfNode rfnode;
		LOG_INFO("%s=%s\n", it->pItem, ini.GetValue("RF_NODES",it->pItem) );
		char tmp[256];
		int rv = sscanf( ini.GetValue("RF_NODES",it->pItem), "%s %i %i", tmp, &rfnode.ackLoggerPort, &rfnode.backbonePort) ;
		rfnode.host = strdup(tmp);
		if ( rv != 3 )
		{
			LOG_INFO("Error - Unable to read ip ackLoggerPort backbonePort\n") ;
			return false ;
		}
		RfNodeMap.insert(std::pair<const char*, RfNode>( strdup(it->pItem), rfnode)) ;
	}
	return true ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Replace the variables contained in XML messages with values specified in the configuration file (ss.ini)
/// or call the appropiate method when the placeholder specifies a function
/// @param line	Message string
/// @param out	Stream containing the message having the variables replaced by values
/// @retval true when the replacement was successfully done
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::expandPlaceHolders(const char* line, std::stringstream& out)
{
	
	if ( !line )
		return false ;

	size_t len = strlen(line) ;
	const char *it = line, *cmdStart ;   // {Hello }
	const char *end = line + len, *paramsStart ;
	
	while ( *line && it != end )
	{
		char cmd[512] = { 0, };
		char params[512] = { 0, };
		
		while ( *it != '{' && it != end )  out.put(*it++) ;
		if ( it == end ) {  return false ; }
		++it ; // skip over {

		cmdStart = it ;
		
		while ( it!=end && isprint(*it) && !isspace(*it) && *it!='}'
			&& *it!=',' )
		{
			++it ;
		}
		if ( it==end || it==cmdStart ) return false ;
			
		memcpy(cmd, cmdStart, it - cmdStart ) ;

		

		while ( it!=end && (isspace(*it) || *it==',' ) ) ++it ;

		paramsStart = it ;

		while ( it!=end && isprint(*it) && *it!='}' )  ++it ;
		if ( it == end ) return false ;

		memcpy(params, paramsStart, it - paramsStart ) ;
				
		std::map<const char*, func_ptr, cmp_str>::iterator cbackIt ;
		cbackIt = placeholderCallbacks.find(cmd) ;
		if ( cbackIt == placeholderCallbacks.end() )
		{
			LOG_INFO("No expansion found for [%s]\n", cmd) ;
			return false ;
		}
		// prepare the stack before calling the method
		// Special case for Config Variables
		if ( cbackIt->second == &ScriptServer::GetConfig )
		{
			Cell cell; cell.Str = strdup(cmd);
		
			m_oDataStack.push( cell ) ;
		}else
			prepareStack(params);

		(this->*placeholderCallbacks[cmd])(out) ;
		++it ;
	}
	
	return true ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Determine the type of a parameter specified for a function placeholder and store it in a parameters list
/// @param str	Parameter string
/// @remarks Called by prepareStack
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::handle(const char* str)
{
	bool isHex=false;

	for(const char*p=str; p!=NULL && *p!='\0';++p)
	{
		if ( isxdigit(*p) )
		{
			isHex = true ;
		}
		else
		{
			isHex = false ;
			break ;
		}
	}

	Cell cell ;
	if ( isHex )
		sscanf( str, "%x", &cell.Int );
	else
		cell.Str = strdup(str) ;

	m_oDataStack.push( cell ) ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Store parameters specified for a function placeholder in XML
/// @param str	Parameters string
////////////////////////////////////////////////////////////////////////////////
void ScriptServer::prepareStack(const char* str )
{
	std::ostringstream token ;
	const char*p=str;
	while ( p!=NULL )
	{

		if ( *p == '\0' || *p == ' ' )
		{
			handle( token.str().c_str() );
			token.str("");
		}
		else
			token << *p ;
		if ( *p == '\0' ) break ;
		++p ;
	}
}

//////////////////////////////////////////////////////////////////////////////////
// CALLBACKS:
//////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the value for the relative TAI placeholder.
/// @param out	TAI value
/// @remarks	TAI is computed using the specified offset and adding a desynchronization value computed as an average of at most (latest) three TAI values received on RX_RF messages.
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::TaiOffset(std::stringstream& out)
{
	
	if ( m_oDataStack.empty() ) return false ;

	int offset = m_oDataStack.top().Int ; m_oDataStack.pop() ;
	
	///compute desync average
	int desync = 0;
	for (int i = 0; i <= taiDesyncQ.size(); i++) {
		desync += taiDesyncQ[i];
	}
	if (desync) {
		desync /= taiDesyncQ.size();
	}

	struct timeval tv ;
	gettimeofday(&tv, (struct timezone*)NULL) ;
	out << std::hex << ( tv.tv_sec+(0x16925E80+34) + offset + desync);
	
	return true ;

}


bool ScriptServer::didx(std::stringstream& out)
{
	out << std::hex << std::setw(4) << std::setfill('0') << g_oCfg.loopIdx ;
}

bool ScriptServer::didxExtdluint( std::stringstream& out )
{
	
	out << std::setw(2) << std::setfill('0') << std::hex;
	printf("IDX:%i\n", g_oCfg.loopIdx );
	if ( g_oCfg.loopIdx < 0x80 )
	{
		out << (g_oCfg.loopIdx << 1 );
	}
	else
	{
		out << (((g_oCfg.loopIdx << 1) | 0x01) & 0xFF) << " ";
		out << std::setw(2) << std::setfill('0') << ((g_oCfg.loopIdx >> 7 ) & 0xFF );
	}
}

bool ScriptServer::load(std::stringstream& out )
{
	
	if ( m_oDataStack.empty() ) return false ;
	int offset,start ;

	if ( m_oDataStack.size() >= 2 )
	{      offset = m_oDataStack.top().Int ; m_oDataStack.pop() ; }
	const char* var = m_oDataStack.top().Str ; m_oDataStack.pop();
	if ( g_oCfg.StorageMap.find( (char*)var ) == g_oCfg.StorageMap.end() )
	{
		LOG_ERROR("Error - LOAD: refence to undefined variable:%s\n",(char*)var );
		return false;
	}

	sscanf( g_oCfg.StorageMap[ (char*)var ], "%x", &start);
	start+=offset ;
	if ( g_oCfg.StorageMap.find( (char*)var ) != g_oCfg.StorageMap.end() )
	{
		out << std::hex << start ;
		LOG_INFO("Loaded:%x\n",start );
		return true ;
	}

	return false ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the value of a placeholder exported from ss.ini.
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::GetConfig(std::stringstream& out)
{
	
	if ( m_oDataStack.empty() ) return false ;

	const char* met = m_oDataStack.top().Str; m_oDataStack.pop();

	CSimpleIniA ini(false, true, true) ;
	ini.LoadFile("../../Config/ss.ini") ;
	const char * rv = ini.GetValue("export", met) ;
	out << rv ;
	
	return false ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the value of a stored id and return it to data of the wait tag for comparison- Kiran J
////////////////////////////////////////////////////////////////////////////////
bool ScriptServer::getStoredCompare(struct Tagwait&w, char *data)
{
	
	int srcCommas, dstCommas,inType,counter=0 ;
long int matchdata,newvalue,iddata;
char operator1;
int ll=strlen(data);
	char* src,*OrigData=new char[ll];
       /*            while(counter<ll)
{
OrigData[counter]=data [counter];
counter++;
}
OrigData[counter]=0;

counter=0;*/
strcpy(OrigData,data);
LOG_INFO("OrigData1=%s", OrigData) ;
		if ( w.id )
		{
			if ( g_oCfg.StorageMap.find( w.id ) == g_oCfg.StorageMap.end() )
			{
				LOG_INFO("ERROR: undefined reference to ID:%s. It was not saved previously\n", w.id ) ;
				//Modified By Honeywell - RK Praveen
				//return false to true - change is made to continue for failcontinue to pass though buffer is empty
				return false ;
			}
			src = g_oCfg.StorageMap[w.id] ; // check to see if the id is in the map
			LOG_INFO("\tLoad Saved content:[id:%s]<%s>\n", w.id, g_oCfg.StorageMap[w.id]) ;
                   while(counter<w.size)
{
data[counter]=src [counter];
counter++;
}

data[counter]=0;
if(strlen(OrigData)!=0)
{
LOG_INFO("OrigData2=%s", OrigData) ;
sscanf( OrigData, "%c%lx",&operator1,&matchdata );
LOG_INFO("Operator=%c Value=%lx", operator1,matchdata) ;
sscanf(data, "%lx",&iddata);
switch(operator1)
{
case '+':
newvalue=iddata+matchdata;
break;
case '-':
newvalue=iddata-matchdata;
break;
case '*':
newvalue=iddata*matchdata;
break;
case '/':
newvalue=iddata/matchdata;
break;
default:
LOG_INFO("Error:Not a valid operator%c ", operator1) ;
break;
}
//itoa(newvalue,data);
//sscanf(newvalue, "%s",data);
sprintf(data, "%lx", newvalue);
if(strlen(data)<w.size)
{
int k=w.size-strlen(data),j=0,n=0;
char temp[w.size+1];
while(j<k)
{
temp[j]='0';
j++;
}
while(j<w.size)
{
temp[j]=data[n];
j++;
n++;
}
temp[j]=0;
memcpy(data,temp,w.size);

}
int kk=0;
	while(data[kk]!='\0')
	{
		if(data[kk]>=97) data[kk]-=32;
		kk++;
	}

}
//LOG_INFO("\tCopied compare data from id:  Data:%s \n",data) ;


			return true;

		}
int kk=0;
	while(data[kk]!='\0')
	{
		if(data[kk]>=97) data[kk]-=32;
		kk++;
	}

//data=strupr(data);
//LOG_INFO("\tCopied compare data from id:  Data:%s \n",data) ;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the binary value of Hexadecimal character in half byte- Kiran J
////////////////////////////////////////////////////////////////////////////////
char* ScriptServer::getBinary(char value)
{
char *ch;
switch(value)
{
case '0':
memcpy(ch,"0000",4);
break;
case '1':
memcpy(ch,"0001",4);
break;
case '2':
memcpy(ch,"0010",4);
break;
case '3':
memcpy(ch,"0011",4);
break;
case '4':
memcpy(ch,"0100",4);
break;
case '5':
memcpy(ch,"0101",4);
break;
case '6':
memcpy(ch,"0110",4);
break;
case '7':
memcpy(ch,"0111",4);
break;
case '8':
memcpy(ch,"1000",4);
break;
case '9':
memcpy(ch,"1001",4);
break;
case 'A':
memcpy(ch,"1010",4);
break;
case 'B':
memcpy(ch,"1011",4);
break;
case 'C':
memcpy(ch,"1100",4);
break;
case 'D':
memcpy(ch,"1101",4);
break;
case 'E':
memcpy(ch,"1110",4);
break;
case 'F':
memcpy(ch,"1111",4);
break;

}
//LOG_INFO( "Ch=[%s] value=%c\n", ch,value) ;
return ch;
}


