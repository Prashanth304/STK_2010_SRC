#include "Flog.h"

int main()
{
	g_stFlog.SetLogLevel(LL_FATAL);
	LOG_INFO("Hello %d ", 4,"\n");
	return 0;
}
