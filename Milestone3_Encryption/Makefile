dchat: executable
CC=g++

executable: broadcast_message.cpp \
	client_chat_ack.cpp \
	msg_listener.cpp \
	user_listener.cpp \
	heartbeat.cpp main.cpp
	$(CC) -g -std=c++11 -o dchat \
	broadcast_message.cpp \
	client_chat_ack.cpp \
	msg_listener.cpp \
	user_listener.cpp \
	heartbeat.cpp \
	main.cpp \
	-pthread

clean: 
	rm dchat

