run: simplehttpd.c config.h simplehttpd.h request.h
	gcc simplehttpd.c semlib.c -lpthread -D_REENTRANT -Wall -o run
