run: simplehttpd.c config.h simplehttpd.h request.h
	gcc simplehttpd.c -Wall -o run  -lpthread -D_REENTRANT
