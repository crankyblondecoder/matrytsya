#include "OllamaModel.hpp"

#include "ModelProvider.hpp"

OllamaModel::OllamaModel(Handle<ModelProvider> provider, std::string name, std::string description,
	std::string inputTokenCost, std::string outputTokenCost, bool free) :
	Model(provider, name, description, inputTokenCost, outputTokenCost, free)
{
}

std::string OllamaModel::processRequest(ModelRequest& request)
{
	return _processRequest(request);
}
