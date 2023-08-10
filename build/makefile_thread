srcFiles = Thread.cpp ThreadCondition.cpp ThreadLock.cpp ThreadMutex.cpp
hdrFiles = Thread.hpp ThreadCondition.hpp ThreadLock.hpp ThreadMutex.hpp

objFiles = $(srcFiles:.cpp=.o)

CXX = $(cname)
CXXFLAGS = $(cflags)

thread.a : $(objFiles)
	@ echo
	@ echo "*** Building Thread Module ***"
	@ echo
	ar rcs thread.a $(objFiles)

# During clean ignore make.dep being rebuilt during only to be removed again.
ifneq ($(MAKECMDGOALS),clean)
   include make.dep
endif

.PHONY : dep
dep : make.dep

make.dep : $(srcFiles) $(hdrFiles)
	@ echo
	@ echo "*** Building Thread Dependencies ***"
	@ echo
	$(cname) $(sdl_cflags) $(depflags) -MM $(srcFiles) > make.dep
	@ echo
	@ echo "*** Thread Dependencies Built Okay ***"
	@ echo

.PHONY : clean
clean :
	@ echo
	@ echo "*** Cleaning Thread Module ***"
	@ echo
	-rm -f *.o make.dep thread.a
