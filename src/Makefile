#CC = clang++ -ggdb3 -Wall -std=c++0x
#CC = g++ -ggdb3 -Wall -std=c++0x
CC = g++ -ggdb3 -Wall -std=c++14

PROTOBUF_SRC = 		\
	root.proto	\
	leaf.proto	\

PROTOBUF_C = $(PROTOBUF_SRC:%.proto=%.pb.cc)
PROTOBUF_H = $(PROTOBUF_SRC:%.proto=%.pb.h)

SRC = 			\
	$(PROTOBUF_C)	\
	base64.cc	\
	compress.cc	\
	crypt.cc	\
	file.cc		\
	file_util.cc	\
	leaf.cc		\
	leaf_proxy.cc	\
	leaf_proxy_map.cc \
	lock.cc		\
	mode.cc		\
	root.cc		\

HEADER = srd.h $(PROTOBUF_H)

OBJECT = $(SRC:%.cc=%.o)

LIBS = 				\
	-lboost_program_options \
	-lbz2			\
	-lcrypto++		\
	-lprotobuf		\

all : srd test TAGS

%.o : %.cc %.h
	$(CC) -c -fpic -o $@ $<

%.o : %.cc
	$(CC) -c -fpic -o $@ $<

%.pb.h %.pb.cc : %.proto
	protoc --cpp_out=. $<

srd : $(HEADER) main.o $(OBJECT) Makefile
	$(CC) -o srd main.o $(OBJECT) $(LIBS)

############################################################
# These rules aren't executed by default, they're here to
# document how to build the app from libraries.  The point
# is to make it easier to build other apps from libsrd.
#
# To build srd from srdlib.a:
srd-from-archive :
	$(CC) -o srd-from-archive main.o libsrd.a $(LIBS)
#
# To build srd from srdlib.so:
srd-from-so :
	$(CC) -o srd-from-so main.o -L. -lsrd $(LIBS)
#
############################################################

python.o : python.cc
	$(CC) -c -fpic -o $@ -I/usr/include/python2.6 $<

libpysrd.so : $(OBJECT) python.o
	$(CC) -shared -Wl,-soname,libpysrd.so -o pysrd.so	\
		python.o					\
		$(OBJECT) -lpython2.6 -lboost_python $(LIBS)

TESTS = 			\
	base64_test		\
	compress_test		\
	crypt_test 		\
	file_test 		\
	file_util_test 		\
	leaf_test 		\
	leaf_proxy_test 	\
	lock_test		\
	mode_test 		\
	root_test 		\

test : $(TESTS)
	./test.sh
	./test-passwd.sh
	./test-delete.sh
	./test-binary.sh
	./test-key-change.sh

%_test : %_test.o test_text.o mode.o $(OBJECT)
	$(CC) -o $@ $^ $(LIBS)
	-./$@

clean : clean-test
	rm -f $(OBJECT) *.o *~ srd TAGS *_test $(PROTOBUF_C) $(PROTOBUF_H)

clean-test :
	rm -rf srd-test-*/ test_[0-9]*\.[0-9]*

TAGS : *cc *h
	etags *cc *h



