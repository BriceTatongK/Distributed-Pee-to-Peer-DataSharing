# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
#che dipende dai target client e server scritti sotto
all: peer ds clean

# make rule per il client
peer: peer.o
	gcc -Wall peer.o -o peer

# make rule per il server
ds: ds.o
	gcc -Wall ds.o -o ds

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o