#ifndef HARNESS_PROVIDER_DESCRIPTOR_H
#define HARNESS_PROVIDER_DESCRIPTOR_H

#include <string>

/**
 * Format-agnostic description of a single model provider, as supplied by a HarnessLoader and consumed
 * by HarnessBuilder. Fields not relevant to this descriptor's type are left at their defaults.
 */
struct HarnessProviderDescriptor
{
	/// Identifies which concrete ModelProvider subclass this descriptor describes.
	enum Type
	{
		OLLAMA
	};

	/// Concrete provider type this descriptor describes.
	Type type;

	/// Name that uniquely identifies this provider within its harness. Local to the loaded data: model
	/// assignments name the provider they draw their model from by it.
	std::string name;

	/// URL of the server, including scheme and port (e.g. "http://localhost:11434").
	std::string url;
};

#endif
