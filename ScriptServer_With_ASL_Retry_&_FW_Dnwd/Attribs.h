#ifndef _ATTRIBS_H_
#define _ATTRIBS_H_
#include <limits.h>

/// The supported message types will be:
//   TX_RF, TX_CFG, TX_RSP,RX_RF, RX_CFG, RX_ERR, RX_DLL_CFM, RX_RECV_DLL_ACK
//   , RX_SENT_DLL_ACK,
//
/// Message structure. (see MsgLayout).
/// TX_RF ,Message number, APDU, TL encryption, priority, discard eligible, ECN
//   , IPv6 src address, IPv6 dst address, IPv6 src port, IPv6 dst port
//   , Contract ID, UDP compression, Network layer header
//   , Link related message, DLL layer header
/// TX_RSP,Message number, APDU, TL encryption, priority, discard eligible, ECN
//   , IPv6 src address, IPv6 dst address, IPv6 src port, IPv6 dst port
//   , Contract ID, UDP compression, Network layer header
//   , Link related message, DLL layer header, Orig APDU, Orig TL, Orig NL
//   , Orig DLL
/// TX_CFG,Message number, APDU
/// RX_RF ,Message number, APDU, Transport layer header, Network layer header
//   , DLL layer header
/// RX_CFG,Message number, APDU
/// RX_ERR,Message number, Severity, Stack level, Error code, Error description


/**
 * @see getMsgType
 * <MSG><Type>TX_RF</Type></MSG>
 * <match type="RX_CFG">
 */
enum MSG_TYPE {
	RX_ERR,
	RX_CFG,
	RX_RF,
	RX_RSP,
	TX_ERR,
	TX_CFG,
	TX_RF,
	TX_RSP,
	RX_DLL_CFM,
	RX_RECV_DLL_ACK,
	RX_SENT_DLL_ACK,
	MSG_UNKNOWN
};
static struct  {
	MSG_TYPE type ;
	const char* tokenName ;
	size_t      tokenLength;
} g_MsgTypes[] = {
	{RX_ERR,"RX_ERR",6},
	{RX_CFG,"RX_CFG",6},
	{RX_RF, "RX_RF",5},
	{RX_RSP,"RX_RSP",6},
	{TX_ERR,"TX_ERR",6},
	{TX_CFG,"TX_CFG",6},
	{TX_RF, "TX_RF",5},
	{TX_RSP,"TX_RSP",6},
	{RX_DLL_CFM,"RX_DLL_CFM",10},
	{RX_RECV_DLL_ACK,"RX_RECV_DLL_ACK",15},
	{RX_SENT_DLL_ACK,"RX_SENT_DLL_ACK",15},
	{MSG_UNKNOWN,NULL,0}
};


/**
 * @see ScriptServer::getPolicyType
 * <MSG policy="norecv">
 */
enum POLICY_TYPE
{
	POLICY_NOMATCH_DROP=1<<1L,
	POLICY_NORECV=1<<2L,
	POLICY_NOSEND=1<<3L,
       POLICY_UNKNOWN=1<<5L,
	POLICY_WAIT=1<<6L,
        POLICY_FAILPASS=1<<4L,
	 POLICY_FAILCONTINUE=1<<7L
} ;
static struct {
	POLICY_TYPE type ;
	const char* tokenName ;
	size_t tokenLength;
} g_PolicyTypes[] = {
	{POLICY_NOMATCH_DROP,"nomatchdrop",11},
	{POLICY_NOMATCH_DROP,"drop",4},
	{POLICY_NORECV,"norecv",6},
	{POLICY_NOSEND,"nosend",6},
	{POLICY_FAILPASS,"failpass",8},
	{POLICY_UNKNOWN,NULL,0},
	{POLICY_WAIT,"wait",4},
       {POLICY_FAILCONTINUE,"failcontinue",4}
};

/**
 * @see ScriptServer::getLayerType
 * <match layer="APP">
 */
enum FIELD_TYPE
{
	FIELD_APP,
	FIELD_TL,
	FIELD_NL,
	FIELD_DLL,
	FIELD_ORIG_APP,
	FIELD_ORIG_TL,
	FIELD_ORIG_NL,
	FIELD_ORIG_DLL,
	FIELD_IPv6_SRC_ADDR,
	FIELD_IPv6_DST_ADDR,
	FIELD_IPv6_SRC_PORT,
	FIELD_IPv6_DST_PORT,
	FIELD_CONTRACT_ID,
	FIELD_UDP_COMPRESSION,
	FIELD_TLENCRYPT,
	FIELD_PRIORITY,
	FIELD_ECN,
	FIELD_UNKNOWN
} ;
static struct {
	FIELD_TYPE type ;
	const char* tokenName ;
	size_t tokenLength;
} g_LayerTypes[] = {
	{FIELD_APP,  "APP",3},
	{FIELD_TL,   "TL",2},
	{FIELD_NL,   "NL",2},
	{FIELD_DLL,  "DLL",3},
	{FIELD_ORIG_APP,       "OrigAPP",7},
	{FIELD_ORIG_TL,        "OrigTL",6},
	{FIELD_ORIG_DLL,       "OrigDLL",7},
	{FIELD_IPv6_SRC_ADDR,  "IPv6SrcAddr",11},
	{FIELD_IPv6_DST_ADDR,  "IPv6DstAddr",11},
	{FIELD_IPv6_SRC_PORT,  "IPv6SrcPort",11},
	{FIELD_IPv6_DST_PORT,  "IPv6DstPort",11},
	{FIELD_CONTRACT_ID,    "ContractID" ,10},
	{FIELD_UDP_COMPRESSION,"UDPCompression" ,14},
	{FIELD_TLENCRYPT,      "TLEncrypt",  9},
	{FIELD_PRIORITY,       "Priority",   8},
	{FIELD_ECN,            "ECN",        3},
	{FIELD_UNKNOWN,NULL}
};

/**
 * @see ScriptServer::getOpType
 * <match operator="lt">
 */
enum OPERATOR_TYPE {
	OP_LT, //lower than
	OP_LE, // lower or equal
	OP_GT, // greater than
	OP_GE, // greater or equal
	OP_EQ, // equal
	OP_UNKNOWN
} ;
static struct {
	OPERATOR_TYPE type ;
	const char*   tokenName ;
	size_t tokenLength ;
} g_OperatorTypes[] = {
	{OP_LT,"lt",2},
	{OP_LE,"le",2},
	{OP_GT,"gt",2},
	{OP_GE,"ge",2},
	{OP_EQ,"eq",2},
	{OP_UNKNOWN,NULL,0}
} ;

/**
 * @see ScriptServer::match
 * match() return code
 */
enum MATCH_TYPE
{
	MATCH_DROP,
	MATCH_FAILED,
	MATCH_OK,
	MATCH_UNKNOWN
} ;

static enum FIELD_TYPE TX_RF_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	FIELD_TLENCRYPT,
	FIELD_PRIORITY,
	FIELD_UNKNOWN,
	FIELD_ECN,
	FIELD_IPv6_SRC_ADDR,
	FIELD_IPv6_DST_ADDR,
	FIELD_IPv6_SRC_PORT,
	FIELD_IPv6_DST_PORT,
	FIELD_CONTRACT_ID,
	FIELD_UDP_COMPRESSION,
	FIELD_NL,
	FIELD_TL,
	FIELD_DLL,
	(FIELD_TYPE)INT_MAX
};
static enum FIELD_TYPE TX_RSP_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	FIELD_TLENCRYPT,
	FIELD_PRIORITY,
	FIELD_UNKNOWN,
	FIELD_ECN,
	FIELD_IPv6_SRC_ADDR,
	FIELD_IPv6_DST_ADDR,
	FIELD_IPv6_SRC_PORT,
	FIELD_IPv6_DST_PORT,
	FIELD_CONTRACT_ID,
	FIELD_UDP_COMPRESSION,
	FIELD_NL,
	FIELD_TL,
	FIELD_DLL,
	FIELD_ORIG_APP,
	FIELD_ORIG_TL,
	FIELD_ORIG_NL,
	FIELD_ORIG_DLL,
	(FIELD_TYPE)INT_MAX
};

static enum FIELD_TYPE TX_CFG_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	(FIELD_TYPE)INT_MAX
};
static enum FIELD_TYPE RX_RF_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	FIELD_TL,
	FIELD_NL,
	FIELD_DLL,
	(FIELD_TYPE)INT_MAX
};

static enum FIELD_TYPE RX_CFG_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	(FIELD_TYPE)INT_MAX
};

static enum FIELD_TYPE RX_ERR_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_APP,
	(FIELD_TYPE)INT_MAX
};

static enum FIELD_TYPE RX_RECV_DLL_ACK_LAYOUT[] = {
	FIELD_UNKNOWN,
	FIELD_UNKNOWN,
	FIELD_DLL,
	(FIELD_TYPE)INT_MAX
};

static enum FIELD_TYPE RX_DLL_CFM_LAYOUT[] = {
      FIELD_UNKNOWN,
      FIELD_UNKNOWN,
      FIELD_UNKNOWN,
      FIELD_UNKNOWN,
      FIELD_UNKNOWN,
      FIELD_DLL,
      (FIELD_TYPE)INT_MAX
};

/**
 * @see MSG_TYPE.
 * Each MSG_TYPE element indexes an entry in this array.
 * The entry describes the structure of a message of type 'index'.
 */
static enum FIELD_TYPE* MsgLayout[] = {
	RX_ERR_LAYOUT,
	RX_CFG_LAYOUT,
	RX_RF_LAYOUT,
	NULL,
	NULL,
	TX_CFG_LAYOUT,
	TX_RF_LAYOUT,
	TX_RSP_LAYOUT,
	RX_DLL_CFM_LAYOUT,
	RX_RECV_DLL_ACK_LAYOUT,
	NULL,
	NULL,
	NULL,
};


#endif	/* _ATTRIBS_H_ */
