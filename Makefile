CC=g++
MAKE=make
OUT=lenabot
OBJ=main.o vkapi.o vkrequest.o bot.o event.o events.o service.o dbg.o list.o	\
	database.o crash.o services.o stats.o
OBJ_DIR=obj/
LIB=-Ljson/ -pthread -lcurl -ljson -lsqlite3
INCLUDE=-Ijson/
CFLAGS=$(INCLUDE) -g -std=c++11 -D_BSD_SOURCE -D_GNU_SOURCE -DUSE_TOR

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $(OBJ_DIR)$@

all: $(OBJ) $(LIB_DEPENDS)
	$(CC) $(CFLAGS) $(addprefix $(OBJ_DIR),$(OBJ)) -o $(OUT) $(LIB)