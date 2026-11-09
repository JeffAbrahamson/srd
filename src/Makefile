# Optimize by default:  besides speed, the compiler's hardening
# (_FORTIFY_SOURCE's checked string and memory functions) only works
# in optimized builds.  For an unoptimized build, say for debugging,
# run "make OPTIMIZE=-O0".
OPTIMIZE ?= -O2

#CC = clang++ $(OPTIMIZE) -ggdb3 -Wall -Wextra -std=c++23 $(EXTRA_FLAGS)
#CC = g++ $(OPTIMIZE) -ggdb3 -Wall -Wextra -std=c++23 $(EXTRA_FLAGS)
CC = g++ $(OPTIMIZE) -ggdb3 -Wall -Wextra -std=c++23 $(EXTRA_FLAGS)

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

# Only pattern rules mention the generated .pb.cc files, so make would
# delete them as intermediate after a build, and the next build would
# regenerate them and rebuild srd.
.SECONDARY : $(PROTOBUF_C) $(PROTOBUF_H)

srd : $(HEADER) main.o $(OBJECT) Makefile
	$(CC) -o srd main.o $(OBJECT) $(LIBS)

END_TO_END_TESTS =		\
	test.sh			\
	test-case.sh		\
	test-passwd.sh		\
	test-delete.sh		\
	test-binary.sh		\
	test-format.sh		\
	test-upgrade.sh		\
	test-key-change.sh	\
	test-readonly.sh	\
	test-edit.sh		\
	test-write-failure.sh	\
	test-errors.sh		\
	test-validate.sh	\
	test-duplicates.sh	\
	test-large.sh		\
	test-concurrency.sh	\
	test-import.sh		\
	test-tty.py		\

# Each test leaves a .passed stamp when it succeeds, so that "make
# test" reruns only the tests whose inputs have changed since.  A
# failing test leaves no stamp and so runs again next time.  The
# test.d directories count as inputs, as deleting a file changes only
# its directory.  Symbolic links don't, so that an editor's lock file
# (a dangling link) can't break the build.
UNIT_STAMPS = $(TESTS:%=%.passed)
END_TO_END_STAMPS = $(END_TO_END_TESTS:%=%.passed)
END_TO_END_INPUTS = srd test_editor.sh $(shell find test.d -type d -o -type f)

# Unit tests first, then the end-to-end tests one at a time in the
# order listed, even under -j:  the end-to-end tests share a database
# directory, and their clean-test would remove the unit tests'
# directories.  Order-only prerequisites chain them; .NOTPARALLEL
# would also do it in make 4.4, but before 4.4 it serialises the whole
# build.  The stamps are left out of clean-test, which the end-to-end
# tests run themselves.  (Phony, or make would build a file "test"
# from test.sh by its built-in rule.)
chain = $(if $(word 2,$1),$(eval $(word 2,$1) : | $(firstword $1))$(call \
	chain,$(wordlist 2,$(words $1),$1)))
$(firstword $(END_TO_END_STAMPS)) : | $(UNIT_STAMPS)
$(call chain,$(END_TO_END_STAMPS))
.PHONY : test test-sanitize clean clean-test
test : srd $(UNIT_STAMPS) $(END_TO_END_STAMPS)

$(UNIT_STAMPS) : %.passed : %
	./$<
	touch $@

$(END_TO_END_STAMPS) : %.passed : % $(END_TO_END_INPUTS)
	./$<
	touch $@

# Linking a unit test doesn't run it:  its .passed stamp does that.
%_test : %_test.o test_text.o mode.o $(OBJECT)
	$(CC) -o $@ $^ $(LIBS)

test-sanitize :
	$(MAKE) clean
	$(MAKE) EXTRA_FLAGS="$(SANITIZE)" test
	$(MAKE) clean

clean : clean-test
	rm -f $(OBJECT) *.o *.dep *~ srd TAGS *_test $(PROTOBUF_C) $(PROTOBUF_H) \
		*.passed

clean-test :
	rm -rf srd-test-*/ test_[0-9]*\.[0-9]*

TAGS : *cc *h
	etags *cc *h


