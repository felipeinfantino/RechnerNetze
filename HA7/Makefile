CC=gcc

all: peer_HA5 peer_HA6

peer_ha6: peer_HA5.c
	$(CC) -g -o peer_ha6 -pthread
	
peer_ha5: peer_HA6.c
	$(CC) -g -o peer_ha5 -pthread
clean:
	rm peer_ha6 peer_ha5

