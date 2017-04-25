#Brandon Sultana, Zach Ziccardi, Mannan Kasliwal

all:	ssfs_mkdsk

ssfs_mkdsk:	ssfs_mkdsk.cpp
	g++ ssfs_mkdsk.cpp -o ssfs_mkdsk

test:	ssfs_mkdsk
	./ssfs_mkdsk 1024 512 DISK

clean:
	rm -f *.o ssfs_mkdsk
