#include "OpenRouterModel.hpp"

#include "ModelProvider.hpp"

OpenRouterModel::OpenRouterModel(Handle<ModelProvider> provider, std::string name, std::string description,
	std::string inputTokenCost, std::string outputTokenCost, bool free) :
	Model(provider, name, description, inputTokenCost, outputTokenCost, free)
{
}

std::string OpenRouterModel::processRequest(ModelRequest& request)
{
	return _processRequest(request);
}
