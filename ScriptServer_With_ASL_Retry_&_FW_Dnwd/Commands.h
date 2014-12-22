#ifndef _COMMANDS_H_
#define _COMMANDS_H_

/*
DEVEUI64
DEVSHORTADDR
SSADDR
SSPORT
SMADDR
DLLKEYID
DLLKEY
ROLECAPA
*/
bool GetConfig(const char* varName, void* out, size_t outSz)
{
	// CIniParser oIni;
	// oIni.Load("ss.ini");
	// oIni.GetVar(varName);
	printf(" search for [%s] in config\n", varName);
}

#endif	 /*_COMMANDS_H_ */
