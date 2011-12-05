GCC = g++ -ggdb3 -Wall

SRC = 			\
	compress.cpp	\
	crypt.cpp	\
	file.cpp	\
	interface.cpp	\
	leaf.cpp	\
	leaf_proxy.cpp	\
	leaf_proxy_map.cpp \
	root.cpp	\

HEADER =		\
	compress.h  	\
	crypt.h  	\
	file.h		\
	interface.h	\
	leaf.h  	\
	leaf_proxy.h	\
	leaf_proxy_map.h \
	root.h  	\
	types.h		\

OBJECT = $(SRC:%.cpp=%.o)

LIBS = 				\
	-lboost_program_options	\
	-lboost_serialization	\
	-lcrypto++		\
	-lbz2			\

all : srd test TAGS

%.o : %.cpp %.h
	$(GCC) -c -o $@ $<

%.o : %.cpp
	$(GCC) -c -o $@ $<

srd : main.o $(HEADER) $(OBJECT) Makefile
	$(GCC) -o srd main.o $(OBJECT) $(LIBS)

test : compress_test crypt_test file_test leaf_test leaf_proxy_test root_test

%_test : %_test.o test_text.o $(OBJECT)
	$(GCC) -o $@ $^ $(LIBS)
	-./$@

clean :
	rm -f $(OBJECT) *.o *~ srd TAGS *_test

clean-test :
	rm -rf srd-test-*/

snap : clean
	-mkdir snapshot/$$(date +'%Y-%m-%d')
	cp *.cpp *.h Makefile snapshot/$$(date +'%Y-%m-%d')

TAGS : *cpp *h
	etags *cpp *h


# Personal to JMA...
time :
	awk -F: '{sum += $$2} END {print sum " minutes (" sum/60 " hours)"}' < time.txt

