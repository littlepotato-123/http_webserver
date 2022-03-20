ALL : server.exe
myArgs = -lpthread -lmysqlclient -g
server.exe : ./log/log.cpp ./log/log.h ./log/LogQueue.h ./pool/threadpool.h ./buffer/buffer.h ./buffer/buffer.cpp ./epoller/epoller.h ./epoller/epoller.cpp ./epoller/server.h ./epoller/server.cpp  ./conn/conn.h ./conn/conn.cpp ./pool/sqlconnpool.h ./pool/sqlconnpool.cpp ./epoller/main.c ./timer/heaptimer.h ./timer/heaptimer.cpp ./conn/response.h ./conn/response.cpp ./conn/request.h ./conn/request.cpp
	g++ $^ -o $@ $(myArgs)

clean:
	-rm -rf server.exe 
.PHONY: clean ALL