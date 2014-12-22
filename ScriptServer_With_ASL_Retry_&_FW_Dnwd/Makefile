all: script_server

script_server: main.cpp ScriptServer.cpp ScriptServer.h Csv.cpp Csv.h Misc.cpp Misc.h Flog.cpp Flog.h Attribs.h ConsoleFileSync.h tinyxml.cpp tinyxml.h tinyxmlerror.cpp tinyxmlparser.cpp tinystr.cpp tinystr.h
	g++ -fno-inline -O0 -g -ggdb3 tinyxml.cpp tinyxmlerror.cpp tinyxmlparser.cpp tinystr.cpp Misc.cpp Csv.cpp ScriptServer.cpp main.cpp Flog.cpp -o script_server

clean:
	rm -rf *.o script_server
