all: server_web mkfs ds3ls ds3cat ds3bits

CC = g++
CFLAGS = -g -Werror -Wall -I include -I shared/include -I/usr/local/opt/openssl@1.1/include -I/opt/homebrew/Cellar/openssl@3/3.2.1/include
LDFLAGS = -L /opt/homebrew/Cellar/openssl@3/3.2.1/lib -lssl -lcrypto -pthread
VPATH = shared

OBJS = server.o MyServerSocket.o MySocket.o HTTPRequest.o HTTPResponse.o http_parser.o HTTP.o HttpService.o HttpUtils.o FileService.o dthread.o WwwFormEncodedDict.o StringUtils.o Base64.o HttpClient.o HTTPClientResponse.o MySslSocket.o DistributedFileSystemService.o LocalFileSystem.o Disk.o

DSUTIL_OBJS = Disk.o LocalFileSystem.o

-include $(OBJS:.o=.d)

server_web: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS)

mkfs: mkfs.o
	gcc -o $@ $(CFLAGS) mkfs.o

ds3ls: ds3ls.o $(DSUTIL_OBJS)
	$(CC) -o $@ $(CFLAGS) ds3ls.o $(DSUTIL_OBJS)

ds3cat: ds3cat.o $(DSUTIL_OBJS)
	$(CC) -o $@ $(CFLAGS) ds3cat.o $(DSUTIL_OBJS)

ds3bits: ds3bits.o $(DSUTIL_OBJS)
	$(CC) -o $@ $(CFLAGS) ds3bits.o $(DSUTIL_OBJS)

%.d: %.c
	@set -e; gcc -MM $(CFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@;
	@[ -s $@ ] || rm -f $@

%.d: %.cpp
	@set -e; $(CC) -MM $(CFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@;
	@[ -s $@ ] || rm -f $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f server_web mkfs ds3ls ds3cat ds3bits *.o *~ core.* *.d
