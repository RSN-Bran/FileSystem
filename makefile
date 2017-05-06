#Brandon Sultana, Zach Ziccardi, Mannan Kasliwal

all:	ssfs_mkdsk	ssfs

ssfs_mkdsk:	ssfs_mkdsk.cpp
	g++ ssfs_mkdsk.cpp -o ssfs_mkdsk

ssfs:	ssfs.cpp
	g++ ssfs.cpp -o ssfs

test:	ssfs_mkdsk	ssfs
	./ssfs_mkdsk 1024 512 DISK
	./ssfs DISK thread1.txt thread2.txt thread3.txt

clean:
	rm -f *.o ssfs_mkdsk
	rm -f *.o ssfs
