# WordleClientC
School project on memory management and server-client connection

To run:
Server - compile server.c and main.c, then run with args <port number> <srand seed> <words file> <file length>
	ex: ./a.out 8118 70 wordle_words.txt 5757

Client - in client folder, compile wordle-client.c, then run with arg <port number>
	ex: ./a.out 8118
