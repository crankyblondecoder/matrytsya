# No object files should be present in root of src directory. Everything should be in a module.

# Each module has a subdirectory where the module name and the directory name are the same.
modules = thread graph log

# The name of the static library (.a file) should be modulename.a
export module_libs = $(foreach module, $(modules), $(module).a)

# Module make file names.
module_makefiles = $(foreach module, $(modules), makefile_$(module))

execName = matrytsya
execName_debug = matrytsya_debug

cflags_debug = -std=c++11 -Wall -g -pthread
cflags_release = -std=c++11 -Wall -fPIC -O3 -pthread
cflags_test = -std=c++11 -Wall -g -pthread

export depflags = -std=c++11
export cname = g++

CXX = $(cname)
CXXFLAGS = $(cflags)

# Tell make to search other paths. Separate multiple with ":"
VPATH = ../src

.PHONY : debug
debug : subDirs
	@ echo
	@ echo "*** Building - Debug ***"
	@ echo
	for makeFile in $(module_makefiles); do cd debug;$(MAKE) --makefile=../$$makeFile cflags="$(cflags_debug)"; done
	@ echo
	@ echo "*** Building - Debug Executable ***"
	@ echo
	$(cname) $(cflags_debug) -o $(execName_debug) ../src/$(execName).cpp $(foreach module_lib, $(module_libs), debug/$(module_lib))

.PHONY : release
release : subDirs
	@ echo
	@ echo "*** Building - Release ***"
	@ echo
	for makeFile in $(module_makefiles); do cd release;$(MAKE) --makefile=../$$makeFile cflags="$(cflags_release)"; done
	@ echo
	@ echo "*** Building - Release Executable ***"
	@ echo
	$(cname) $(cflags_release) -o $(execName) ../src/$(execName).cpp $(foreach module_lib, $(module_libs), release/$(module_lib))

.PHONY : clean
clean : subDirs
    # Remember these are shell commands!
	@ echo
	@ echo "*** Clean ***"
	@ echo
	-rm $(execName)
	-rm $(execName_debug)
	-rm -rf ./debug/*
	-rm -rf ./release/*
	-rm -rf ./test/*

.PHONY : test
test : subDirs $(modules)
	@ echo
	@ echo "*** Building and Running Tests ***"
	@ echo
	for makeFile in $(module_makefiles); do cd test;$(MAKE) --makefile=../$$makeFile cflags="$(cflags_test)"; done
	cd test; $(MAKE) --makefile=../makefile_test cflags="$(cflags_test)"

.PHONY : subDirs
subDirs :
	@ mkdir -p debug
	@ mkdir -p release
	@ mkdir -p test
