#ifndef LOG__H
#define LOG__H

#include <iostream>

/** Simple logging mechanism */

// Global for speed of query.
extern Logger* defaultLogger;

// Error messages should always get through.
#define LOG(logLevel, text) defaultLogger -> log(logLevel, text);

class Logger
{
	public:

		enum LogLevel
		{
			/// Errors should always be displayed.
			ERROR,
			/// General information.
			INFO,
			/// Warning. Hint to turn up the log level.
			WARN,
			/// Helpful output for debugging.
			DEBUG,
			/// Turn logging ALL the way up.
			ELEVEN
		};

		Logger(Logger::LogLevel initialLogLevel);

		void log(Logger::LogLevel logLevel, const std::string& message);

	protected:

	private:

		enum LogLevel _logLevel;
};

#endif
