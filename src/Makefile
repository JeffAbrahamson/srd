#CC = clang++ -ggdb3 -Wall -Wextra -std=c++20 $(EXTRA_FLAGS)
#CC = g++ -ggdb3 -Wall -Wextra -std=c++20 $(EXTRA_FLAGS)
CC = g++ -ggdb3 -Wall -Wextra -std=c++20 $(EXTRA_FLAGS)

# Run the tests under AddressSanitizer and UndefinedBehaviorSanitizer.
# Objects built with and without sanitizers can't be mixed, so this
# cleans before and after.
SANITIZE = -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	-fno-omit-frame-pointer

PROTOBUF_SRC = 		\
	root.proto	\
	leaf.proto	\

PROTOBUF_C = $(PROTOBUF_SRC:%.proto=%.pb.cc)
PROTOBUF_H = $(PROTOBUF_SRC:%.proto=%.pb.h)

SRC = 			\
	$(PROTOBUF_C)	\
	compress.cc	\
	crypt.cc	\
	file.cc		\
	file_util.cc	\
	header.cc	\
	leaf.cc		\
	leaf_proxy.cc	\
	leaf_proxy_map.cc \
	lock.cc		\
	mode.cc		\
	root.cc		\

HEADER = srd.h $(PROTOBUF_H)

OBJECT = $(SRC:%.cc=%.o)

TESTS = 			\
	compress_test		\
	crypt_test 		\
	file_test 		\
	file_util_test 		\
	header_test		\
	leaf_test 		\
	leaf_proxy_test 	\
	lock_test		\
	mode_test 		\
	root_test 		\

TEST_OBJECTS := $(TESTS:%=%.o)

LIBS = 				\
	-lboost_program_options \
	-lbz2			\
	-lcrypto++		\
	-lprotobuf		\
	-lsodium		\

all : srd test TAGS

# -MMD -MP has the compiler write each object's header dependencies
# to a .dep file, included below, so that changing a header rebuilds
# everything that includes it.  (Not the usual .d, which would catch
# the test.d directory.)
%.o : %.cc
	$(CC) -MMD -MP -MF $*.dep -c -fpic -o $@ $<

# Nothing can compile before the protobuf headers exist.  (After the
# first build, the .dep files record this too.)
$(OBJECT) main.o test_text.o $(TEST_OBJECTS) : $(PROTOBUF_H)

-include $(wildcard *.dep)

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

test : srd $(TESTS)
	./test.sh
	./test-case.sh
	./test-passwd.sh
	./test-delete.sh
	./test-binary.sh
	./test-format.sh
	./test-key-change.sh
	./test-readonly.sh
	./test-edit.sh
	./test-write-failure.sh
	./test-errors.sh
	./test-validate.sh
	./test-duplicates.sh
	./test-large.sh
	./test-concurrency.sh
	./test-import.sh
	./test-tty.py

%_test : %_test.o test_text.o mode.o $(OBJECT)
	$(CC) -o $@ $^ $(LIBS)
	./$@

test-sanitize :
	$(MAKE) clean
	$(MAKE) EXTRA_FLAGS="$(SANITIZE)" test
	$(MAKE) clean

clean : clean-test
	rm -f $(OBJECT) *.o *.dep *~ srd TAGS *_test $(PROTOBUF_C) $(PROTOBUF_H)

clean-test :
	rm -rf srd-test-*/ test_[0-9]*\.[0-9]*

TAGS : *cc *h
	etags *cc *h


