#include "AgentHarness.hpp"

AgentHarness::AgentHarness()
{
}

AgentHarness::~AgentHarness()
{
}

void AgentHarness::addModelAssignment(Role role, Capability capability, AgentModel model)
{
	_assignments.push_back(ModelAssignment{role, capability, model});
}

std::vector<AgentHarness::ModelAssignment> AgentHarness::getModelAssignments()
{
	return _assignments;
}
