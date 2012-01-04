GCC = g++ -ggdb3 -Wall

SRC = 			\
	compress.cpp	\
	crypt.cpp	\
	file.cpp	\
	leaf.cpp	\
	leaf_proxy.cpp	\
	leaf_proxy_map.cpp \
	mode.cpp	\
	root.cpp	\

HEADER =		\
	compress.h  	\
	crypt.h  	\
	file.h		\
	leaf.h  	\
	leaf_proxy.h	\
	leaf_proxy_map.h \
	mode.h		\
	root.h  	\
	types.h		\

OBJECT = $(SRC:%.cpp=%.o)

LIBS = 				\
	-lboost_program_options	\
	-lboost_serialization	\
	-lcrypto++		\
	-lbz2			\

all : libs srd test TAGS

%.o : %.cpp %.h
	$(GCC) -c -fpic -o $@ $<

%.o : %.cpp
	$(GCC) -c -fpic -o $@ $<

srd : main.o $(HEADER) $(OBJECT) Makefile
	$(GCC) -o srd main.o $(OBJECT) $(LIBS)

libs : libsrd.so libsrd.a

libsrd.a : $(OBJECT)
	ar rcs $@ $^

libsrd.so : $(OBJECT)
	gcc -shared -Wl,-soname,$@.1 -o $@.1.0.1 $^


############################################################
# These rules aren't executed by default, they're here to
# document how to build the app from libraries.  The point
# is to make it easier to build other apps from libsrd.
#
# To build srd from srdlib.a:
srd-from-archive :
	$(GCC) -o srd-from-archive main.o libsrd.a $(LIBS)
#
# To build srd from srdlib.so:
srd-from-so :
	$(GCC) -o srd-from-so main.o -L. -lsrd $(LIBS)
#
############################################################

TESTS = 			\
	compress_test		\
	crypt_test 		\
	file_test 		\
	leaf_test 		\
	leaf_proxy_test 	\
	mode_test 		\
	root_test 		\

test : $(TESTS)
	./test.sh
	./test-passwd.sh
	./test-delete.sh

%_test : %_test.o test_text.o mode.o $(OBJECT)
	$(GCC) -o $@ $^ $(LIBS)
	-./$@

clean :
	rm -f $(OBJECT) *.o *~ srd TAGS *_test

clean-test :
	rm -rf srd-test-*/ test_[0-9]*\.[0-9]*

snap : clean
	-mkdir snapshot/$$(date +'%Y-%m-%d')
	cp *.cpp *.h Makefile snapshot/$$(date +'%Y-%m-%d')

TAGS : *cpp *h
	etags *cpp *h



