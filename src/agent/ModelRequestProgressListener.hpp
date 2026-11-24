#ifndef MODEL_REQUEST_PROGRESS_LISTENER_H
#define MODEL_REQUEST_PROGRESS_LISTENER_H

#include <string>

/**
 * Receives reports of the progress a model makes while servicing a request, before the request completes.
 * @note Not reference counted. A listener is only ever called synchronously, on the thread the request was made on
 *       and while that request is still being serviced, so whoever made the request owns it and need only keep it
 *       alive until the request returns.
 */
class ModelRequestProgressListener
{
	public:

		virtual ~ModelRequestProgressListener() {}

		/**
		 * Report progress made on the request being serviced.
		 * @param text Human readable description of the progress, e.g. what the model said and which tools it
		 *        called in one round of tool calls.
		 */
		virtual void agentRequestProgressReport(std::string text) = 0;
};

#endif
