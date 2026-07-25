#include "ModelContext.hpp"

#include "AgentException.hpp"
#include "ModelToolBindings.hpp"
#include "../thread/thread.hpp"

ModelContext::RequestClaim::RequestClaim(ModelContext& context) :
	_context{context}
{
	if(!_context.__claim())
	{
		throw AgentException(AgentException::MODEL_CONTEXT_IN_USE);
	}
}

ModelContext::RequestClaim::~RequestClaim()
{
	_context.__release();
}

std::atomic<unsigned> ModelContext::_nextId{0};

ModelContext::ModelContext(std::vector<ModelSystemPrompt> systemPrompts,
	std::vector<Handle<ModelToolBindings>> tools) :
	_id{_nextId++}, _systemPrompts{systemPrompts}, _tools{tools}
{
}

ModelContext::~ModelContext()
{
}

unsigned ModelContext::getId()
{
	return _id;
}

void ModelContext::setDescription(std::string description)
{
	{ SYNC(_lock)

		_description = description;
	}
}

std::string ModelContext::getDescription()
{
	std::string firstPrompt;

	{ SYNC(_lock)

		if(!_description.empty()) return _description;

		if(!_chatHistory.empty()) firstPrompt = _chatHistory.front().prompt.getPrompt();
	}

	// Nothing was set, so the context is described by what was first asked of it, cut short.
	return firstPrompt.substr(0, MODEL_CONTEXT_DESCRIPTION_LENGTH);
}

std::vector<ModelSystemPrompt> ModelContext::getSystemPrompts()
{
	return _systemPrompts;
}

std::vector<Handle<ModelToolBindings>> ModelContext::getTools()
{
	return _tools;
}

std::vector<ModelContext::ChatExchange> ModelContext::getChatHistory()
{
	std::vector<ChatExchange> chatHistory;

	{ SYNC(_lock)

		chatHistory = _chatHistory;
	}

	return chatHistory;
}

std::string ModelContext::getLastResponse()
{
	std::string lastResponse;

	{ SYNC(_lock)

		if(!_chatHistory.empty())
		{
			lastResponse = _chatHistory.back().response;
		}
	}

	return lastResponse;
}

std::string ModelContext::getReadableChatHistory()
{
	std::vector<ChatExchange> chatHistory = getChatHistory();

	std::string history;

	for(ChatExchange& exchange : chatHistory)
	{
		history += "User: " + exchange.prompt.getPrompt() + "\n";

		for(ToolCallRound& round : exchange.toolCallRounds)
		{
			if(!round.content.empty())
			{
				history += "Assistant: " + round.content + "\n";
			}
		}

		history += "Assistant: " + exchange.response + "\n\n";
	}

	return history;
}

void ModelContext::addChatExchange(ModelPrompt prompt, std::vector<ToolCallRound> toolCallRounds,
	std::string response)
{
	// Built before the lock is taken, as none of it is thread shared until it is in the history.
	ChatExchange exchange{prompt, toolCallRounds, response};

	{ SYNC(_lock)

		_chatHistory.push_back(std::move(exchange));
	}
}

bool ModelContext::__claim()
{
	bool claimed = false;

	{ SYNC(_lock)

		if(!_requestInProgress)
		{
			_requestInProgress = true;

			claimed = true;
		}
	}

	return claimed;
}

void ModelContext::__release()
{
	{ SYNC(_lock)

		_requestInProgress = false;
	}
}
