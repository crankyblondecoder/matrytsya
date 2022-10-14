objFiles = log.o

CXX = $(cname)
CXXFLAGS = $(cflags)

log.a : $(objFiles)
	@ echo
	@ echo "*** Building Log Module ***"
	@ echo
	ar rcs log.a $(objFiles)

# During clean ignore make.dep being rebuilt during only to be removed again.
ifneq ($(MAKECMDGOALS),clean)
   include make.dep
endif

PHONY : dep
dep : make.dep

make.dep : $(objFiles:.o=.hpp)
	@ echo
	@ echo "*** Building Log Dependencies ***"
	@ echo
	  $(cname) $(sdl_cflags) $(depflags) -MM $(objFiles:.o=.cpp) > make.dep
	@ echo
	@ echo "*** Log Dependencies Built Okay ***"
	@ echo

PHONY : clean
clean :
	@ echo
	@ echo "*** Cleaning Log Module ***"
	@ echo
	-rm -f *.o make.dep log.a
