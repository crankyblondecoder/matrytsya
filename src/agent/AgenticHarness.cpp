#include "AgenticHarness.hpp"

#include "AgentException.hpp"
#include "Model.hpp"
#include "ModelContext.hpp"
#include "ModelRequest.hpp"
#include "ModelToolBindings.hpp"

AgenticHarness::AgenticHarness()
{
}

AgenticHarness::~AgenticHarness()
{
}

void AgenticHarness::addModelAssignment(RoleCapability roleCapability, Handle<Model> model)
{
	_assignments.push_back(ModelAssignment{roleCapability, model});
}

std::vector<AgenticHarness::ModelAssignment> AgenticHarness::getModelAssignments()
{
	return _assignments;
}

void AgenticHarness::addSystemPrompt(RoleCapability roleCapability, ModelSystemPrompt systemPrompt)
{
	_systemPrompts.push_back(SystemPromptAssignment{roleCapability, systemPrompt});
}

std::vector<AgenticHarness::SystemPromptAssignment> AgenticHarness::getSystemPrompts()
{
	return _systemPrompts;
}

void AgenticHarness::addToolBinding(std::vector<RoleCapability> roleCapabilities, Handle<ModelToolBindings> tools)
{
	_toolBindings.push_back(ToolBindingAssignment{roleCapabilities, tools});
}

std::vector<AgenticHarness::ToolBindingAssignment> AgenticHarness::getToolBindings()
{
	return _toolBindings;
}

Handle<ModelContext> AgenticHarness::processRequest(std::string prompt, Role role, Capability capability,
	Handle<ModelContext> context)
{
	// Refused before a model is looked up, so that an empty prompt is reported as the fault it is rather
	// than as whatever the lookup fails to find on the way.
	if(prompt.empty())
	{
		throw AgentException(AgentException::EMPTY_MODEL_REQUEST);
	}

	Handle<Model> candidateModel(0);

	for(ModelAssignment& assignment : _assignments)
	{
		if(assignment.roleCapability.role != role) continue;

		// A more capable model can be substituted for the one requested.
		if(assignment.roleCapability.capability < capability) continue;

		candidateModel = assignment.model;

		break;
	}

	if(!candidateModel.isValid())
	{
		throw AgentException(AgentException::NO_CANDIDATE_MODEL);
	}

	// A context carried over from a previous call already has its system prompts, tools and history
	// fixed; only build a new one when the caller is starting a fresh conversation.
	if(!context.isValid())
	{
		std::vector<ModelSystemPrompt> systemPrompts;

		for(SystemPromptAssignment& systemPromptAssignment : _systemPrompts)
		{
			if(systemPromptAssignment.roleCapability.role != role) continue;
			if(systemPromptAssignment.roleCapability.capability != capability) continue;

			systemPrompts.push_back(systemPromptAssignment.systemPrompt);
		}

		std::vector<Handle<ModelToolBindings>> tools;

		for(ToolBindingAssignment& toolBindingAssignment : _toolBindings)
		{
			for(RoleCapability& toolRoleCapability : toolBindingAssignment.roleCapabilities)
			{
				if(toolRoleCapability.role != role) continue;
				if(toolRoleCapability.capability != capability) continue;

				tools.push_back(toolBindingAssignment.tool);

				break;
			}
		}

		ModelContext* newContext = new ModelContext(systemPrompts, tools);

		context = Handle<ModelContext>(newContext);

		// The handle holds the reference the request needs; release the implicit construction ref.
		newContext -> decrRef();
	}

	ModelRequest request(context, ModelPrompt(prompt));

	candidateModel.getInstance() -> processRequest(request);

	return context;
}
