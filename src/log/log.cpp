#include "log.hpp"

#include <iostream>

// For the moment set to max log level.
Logger* defaultLogger = new Logger(Logger::LogLevel::ELEVEN);

Logger::Logger(Logger::LogLevel initialLogLevel)
{
	_logLevel = initialLogLevel;
}

void Logger::log(Logger::LogLevel logLevel, const std::string& message)
{
	if(logLevel <= _logLevel) {

		switch ((logLevel))
		{
			case Logger::LogLevel::ERROR:

				std::cerr <<  "[ERROR] " << message << '\n';
				break;

			case Logger::LogLevel::INFO:

				std::clog << "[INFO] " << message << '\n';
				break;

			case Logger::LogLevel::WARN:

				std::clog << "[WARN] " << message << '\n';
				break;

			case Logger::LogLevel::DEBUG:
				std::clog << "[DEBUG] " << message << '\n';
				break;

			default:
				std::clog << "[WARN] " << message << '\n';
				break;
		}
	}
}
