# No object files should be present in root of src directory. Everything should be in a module.

# Each module has a subdirectory where the module name and the directory name are the same.
#modules = event graph thread log
modules = thread graph
# The name of the static library (.a file) should be modulename.a
export module_libs = $(foreach module, $(modules), ./$(module)/$(module).a)

execName = matrytsya
export cflags = -std=c++11 -Wall -g -pthread
export depflags = -std=c++11
export cname = g++
export sharedLibs =

CXX = $(cname)
CXXFLAGS = $(cflags)

matrytsya : $(modules) $(execName).cpp
	@ echo
	@ echo "*** Building Matrytsya Executable ***"
	@ echo
	$(cname) $(cflags) -o $(execName) $(execName).cpp $(module_libs) $(sharedLibs)

.PHONY: $(modules)
$(modules):
	$(MAKE) -C $@

.PHONY : clean
clean :
	@ echo
	@ echo "*** Cleaning /src Root ***"
	@ echo
	-rm -f $(objFiles) $(execName)
	@ -for dir in $(modules); do $(MAKE) -C $$dir clean; done
	$(MAKE) -C test clean

.PHONY : test
test : $(modules)
	@ echo
	@ echo "*** Building and Running Tests ***"
	@ echo
	$(MAKE) -C test test
