# No object files should be present in root of src directory. Everything should be in a module.

# Each module has a subdirectory where the module name and the directory name are the same.
# The order is important because it sets the final linking order and if that is incorrect it causes undefined reference errors.
modules = graph util thread log lua display mongoose

# The name of the static library (.a file) should be modulename.a
export module_libs = $(foreach module, $(modules), $(module).a)

# Module make file names.
module_makefiles = $(foreach module, $(modules), makefile_$(module))

execName = matrytsya
execName_debug = matrytsya_debug
execName_test = matrytsya_test

cflags_debug = -Wall -std=c++20 -O0 -g -pthread
cflags_release = -Wall -std=c++20 -fPIC -O3 -pthread
cflags_test = -Wall -std=c++20 -O0 -g -pthread

export depflags = -std=c++20
export cname = g++

# Lua's build artifacts (see makefile_lua) land in the same debug/release/test
# directories as every other module's. "clean" needs their names so it can
# leave them alone; only "lua_clean" is allowed to remove them.
luaObjFiles = $(notdir $(wildcard ../src/lua/*.c))
luaObjFiles := $(luaObjFiles:.c=.o)
luaArtifacts = lua.a $(luaObjFiles) $(luaObjFiles:.o=.dep) $(luaObjFiles:.o=.depMake)

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

.PHONY : debug_test
debug_test : subDirs
	@ echo
	@ echo "*** Building - Debug ***"
	@ echo
	for makeFile in $(module_makefiles); do cd debug;$(MAKE) --makefile=../$$makeFile cflags="$(cflags_debug)"; done
	@ echo
	@ echo "*** Building - Debug Test Executable ***"
	@ echo
	$(cname) $(cflags_debug) -o $(execName_test) ../src/$(execName_test).cpp $(foreach module_lib, $(module_libs), debug/$(module_lib))

.PHONY : lua
lua : subDirs
	@ echo
	@ echo "*** Building - Lua ***"
	@ echo
	cd debug;$(MAKE) --makefile=../makefile_lua cflags="$(cflags_debug)"

.PHONY : dep
dep : subDirs
	@ echo
	@ echo "*** Building Dependencies ***"
	@ echo
	for makeFile in $(module_makefiles); do cd debug;$(MAKE) --makefile=../$$makeFile dep cflags="$(cflags_debug)"; done

.PHONY : clean
clean : subDirs
    # Remember these are shell commands!
	@ echo
	@ echo "*** Clean ***"
	@ echo
	@ -rm -f $(execName)
	@ -rm -f $(execName_debug)
	@ -rm -f $(execName_test)
	@ -find ./debug ./release ./test -mindepth 1 $(foreach f, $(luaArtifacts), -not -name $(f)) -delete

.PHONY : lua_clean
lua_clean :
	@ echo
	@ echo "*** Clean - Lua ***"
	@ echo
	@ -rm -f $(foreach dir, debug release test, $(foreach f, $(luaArtifacts), $(dir)/$(f)))

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
