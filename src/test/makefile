srcFiles = unitTests.cpp UnitTest.cpp
hdrFiles = ./thread/ThreadModuleUnitTests.hpp ./thread/TestThread.hpp ./thread/ThreadUnitTest.hpp \
	UnitTest.hpp

objFiles = $(srcFiles:.cpp=.o)

module_lib_paths = $(foreach module, $(module_libs), ../$(module))

CXX = $(cname)
CXXFLAGS = $(cflags)

.PHONY : test
test : $(objFiles)
	$(cname) $(cflags) -o runUnitTests $(objFiles) $(module_lib_paths) $(sharedLibs)
#	./runUnitTests

# During clean ignore make.dep being rebuilt during only to be removed again.
ifneq ($(MAKECMDGOALS),clean)
   include make.dep
endif

.PHONY : dep
dep : make.dep

make.dep : $(srcFiles) $(hdrFiles)
	@ echo
	@ echo "*** Building Test Dependencies ***"
	@ echo
	$(cname) $(depflags) -MM $(srcFiles) > make.dep
	@ echo
	@ echo "*** Test Dependencies Built Okay ***"
	@ echo

.PHONY : clean
clean :
	@ echo
	@ echo "*** Cleaning Unit Tests ***"
	@ echo
	-rm -f *.o runUnitTests
