#ifndef EXCEPTION_H
#define EXCEPTION_H

class Exception
{
	public:

		/// Module that the exception belongs to.
		enum Module
        {
			EVENT,
			THREAD,
			GRAPH
		};

		Exception(Module module) : _module{module} {}

		Module getModule() {return _module;}

	private:

		Module _module;
};

#endif