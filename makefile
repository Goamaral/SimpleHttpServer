all: run console
run: simplehttpd.c config.h simplehttpd.h request.h
	gcc simplehttpd.c -Wall -o run -lpthread -D_REENTRANT
console: console.c simplehttpd.h
	gcc console.c -Wall -o console
