# No object files should be present in root of src directory. Everything should be in a module.

# Each module has a subdirectory where the module name and the directory name are the same.
#modules = event graph thread log
modules = thread graph log

# The name of the static library (.a file) should be modulename.a
module_libs = $(foreach module, $(modules), $(module).a)

# Module make file names.
module_makefiles = $(foreach module, $(modules), makefile_$(module))

execName = matrytsya

cflags_debug = -std=c++11 -Wall -g -pthread
cflags_release = -std=c++11 -Wall -fPIC -O3 -pthread
cflags_test = -std=c++11 -Wall -g -pthread

export depflags = -std=c++11
export cname = g++

CXX = $(cname)
CXXFLAGS = $(cflags)

.PHONY : debug
debug : subdirs
	@ echo
	@ echo "*** Building - debug ***"
	@ echo
	for makeFile in $(module_makefiles); do $(MAKE) -C $$makeFile cflags=$(cflags_debug) outputdir=debug; done


.PHONY : release
release : subdirs $(module_libs) $(execName).cpp
	@ echo
	@ echo "*** Building -  ***"
	@ echo
	$(cname) $(cflags) -o $(execName) $(execName).cpp $(module_libs)

.PHONY : clean
clean : subdirs
    # Remember these are shell commands!
	@ echo
	@ echo "*** Cleaning /src Root ***"
	@ echo
	-rm -f $(objFiles) $(execName)
	@ -for dir in $(modules); do $(MAKE) -C $$dir clean; done
	$(MAKE) -C test clean

.PHONY : test
test : subdirs $(modules)
	@ echo
	@ echo "*** Building and Running Tests ***"
	@ echo
	$(MAKE) -C test test

.PHONY : subdirs
subdirs :
	@ mkdir -p debug
	@ mkdir -p release
	@ mkdir -p test
