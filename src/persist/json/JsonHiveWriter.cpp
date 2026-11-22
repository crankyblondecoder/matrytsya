#include "JsonHiveWriter.hpp"

#include "../HiveNodeDescriptor.hpp"
#include "../HiveSnapshot.hpp"
#include "../HiveSurfaceDescriptor.hpp"
#include "../hiveNames.hpp"
#include "../../rapidjson/stringbuffer.h"
#include "../../rapidjson/writer.h"
#include "../../util/base64.hpp"

#include <cstddef>

namespace
{
	typedef rapidjson::Writer<rapidjson::StringBuffer> HiveJsonWriter;

	void __writeString(HiveJsonWriter& writer, const std::string& value)
	{
		writer.String(value.c_str(), (rapidjson::SizeType) value.size());
	}

	void __writeKey(HiveJsonWriter& writer, const char* key)
	{
		writer.Key(key);
	}

	void __writeDoubleArray(HiveJsonWriter& writer, const double* values, unsigned count)
	{
		writer.StartArray();

		for(unsigned i = 0; i < count; i++) writer.Double(values[i]);

		writer.EndArray();
	}

	void __writeColour(HiveJsonWriter& writer, const std::byte* colour)
	{
		writer.StartArray();

		for(unsigned channel = 0; channel < 4; channel++) writer.Uint(std::to_integer<unsigned>(colour[channel]));

		writer.EndArray();
	}

	void __writeEdges(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		__writeKey(writer, "edges");

		writer.StartArray();

		for(const HiveEdgeDescriptor& edge : descriptor.edges)
		{
			writer.StartObject();

			__writeKey(writer, "toNodeName");
			__writeString(writer, edge.toNodeName);

			__writeKey(writer, "actionFlags");
			writer.StartArray();
			for(const std::string& flagName : edge.actionFlagNames) __writeString(writer, flagName);
			writer.EndArray();

			__writeKey(writer, "actionsCompleteAfterTraverse");
			writer.Bool(edge.actionsCompleteAfterTraverse);

			__writeKey(writer, "traversalKey");
			writer.Uint(edge.traversalKey);

			writer.EndObject();
		}

		writer.EndArray();
	}

	void __writeDestination(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		// The getters are not const, so a copy is read from.
		GraphNodeLocation destination = descriptor.destination;

		__writeKey(writer, "destination");

		writer.StartObject();

		__writeKey(writer, "hostname");
		__writeString(writer, destination.getHostname());

		__writeKey(writer, "port");
		writer.Int(destination.getPort());

		__writeKey(writer, "hiveName");
		__writeString(writer, destination.getHiveName());

		__writeKey(writer, "nodeName");
		__writeString(writer, destination.getNodeName());

		writer.EndObject();
	}

	void __writeVertexGroups(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		// Always the grouped form, which states each group's visibility once rather than against every vertex.
		__writeKey(writer, "vertexGroups");

		writer.StartArray();

		for(const HiveVertexGroupDescriptor& group : descriptor.vertexGroups)
		{
			writer.StartObject();

			__writeKey(writer, "visibility");
			__writeString(writer, group.visibilityName);

			__writeKey(writer, "vertexes");

			writer.StartArray();

			for(const Vertex& vertex : group.vertexes)
			{
				writer.StartObject();

				__writeKey(writer, "posn");
				__writeDoubleArray(writer, vertex.posn, 3);

				__writeKey(writer, "colour");
				__writeColour(writer, vertex.colour);

				__writeKey(writer, "texCoords");
				__writeDoubleArray(writer, vertex.texCoords, 2);

				__writeKey(writer, "normal");
				__writeDoubleArray(writer, vertex.normal, 3);

				writer.EndObject();
			}

			writer.EndArray();

			writer.EndObject();
		}

		writer.EndArray();
	}

	void __writeScriptSource(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		__writeKey(writer, "coreScript");
		__writeString(writer, descriptor.coreScript);

		__writeKey(writer, "pokeScript");
		__writeString(writer, descriptor.pokeScript);
	}

	void __writeAgentFields(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		__writeKey(writer, "capability");
		__writeString(writer, descriptor.capabilityName);

		__writeKey(writer, "prompts");

		writer.StartArray();

		for(const HiveAgentPromptDescriptor& prompt : descriptor.prompts)
		{
			writer.StartObject();

			__writeKey(writer, "nodeIdentifier");
			__writeString(writer, prompt.nodeIdentifier);

			__writeKey(writer, "nodeType");
			__writeString(writer, prompt.nodeTypeName);

			__writeKey(writer, "prompt");
			__writeString(writer, prompt.prompt);

			__writeKey(writer, "terminateOnResponse");
			writer.Bool(prompt.terminateOnResponse);

			writer.EndObject();
		}

		writer.EndArray();

		__writeKey(writer, "autoTriggerAgentAction");
		writer.Bool(descriptor.autoTriggerAgentAction);

		__writeKey(writer, "serialiseEmittedActions");
		writer.Bool(descriptor.serialiseEmittedActions);
	}

	void __writeBackground(HiveJsonWriter& writer, const TextBackground& background)
	{
		writer.StartObject();

		__writeKey(writer, "fillColour");
		__writeColour(writer, background.fillColour);

		__writeKey(writer, "borderColour");
		__writeColour(writer, background.borderColour);

		__writeKey(writer, "borderWidth");
		writer.Double(background.borderWidth);

		__writeKey(writer, "padding");
		writer.Double(background.padding);

		__writeKey(writer, "cornerRadius");
		__writeDoubleArray(writer, background.cornerRadii, TextBackground::CORNER_COUNT);

		writer.EndObject();
	}

	void __writeImageFields(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		// A file or URL image is written as the reference it was given rather than its bytes, so that a stored hive
		// goes on following the file or URL.
		switch(descriptor.imageKind)
		{
			case ImageSource::EMBEDDED:
				__writeKey(writer, "imageData");
				__writeString(writer, base64::encode(descriptor.imageBytes));
				break;

			case ImageSource::FILE:
				__writeKey(writer, "imageFile");
				__writeString(writer, descriptor.imageReference);
				break;

			case ImageSource::URL:
				__writeKey(writer, "imageUrl");
				__writeString(writer, descriptor.imageReference);
				break;
		}

		__writeKey(writer, "imagePosition");
		__writeDoubleArray(writer, descriptor.imagePosition, 3);

		__writeKey(writer, "imageHeight");
		writer.Double(descriptor.imageHeight);

		__writeKey(writer, "imageBackground");
		__writeBackground(writer, descriptor.imageBackground);

		__writeKey(writer, "imageVisible");
		writer.Bool(descriptor.imageVisible);
	}

	void __writeTextFields(HiveJsonWriter& writer, const HiveNodeDescriptor& descriptor)
	{
		__writeKey(writer, "text");
		__writeString(writer, descriptor.text);

		__writeKey(writer, "textPosition");
		__writeDoubleArray(writer, descriptor.textPosition, 3);

		__writeKey(writer, "textHeight");
		writer.Double(descriptor.textHeight);

		__writeKey(writer, "textColour");
		__writeColour(writer, descriptor.textColour);

		__writeKey(writer, "textBackground");
		__writeBackground(writer, descriptor.textBackground);

		__writeKey(writer, "textVisible");
		writer.Bool(descriptor.textVisible);
	}

	std::string __strobeFragment(const char* nameKey, const std::string& name, unsigned periodMs)
	{
		rapidjson::StringBuffer buffer;
		HiveJsonWriter writer(buffer);

		writer.StartObject();

		__writeKey(writer, nameKey);
		__writeString(writer, name);

		__writeKey(writer, "periodMs");
		writer.Uint(periodMs);

		writer.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize());
	}
}

std::string JsonHiveWriter::nodeFragment(const HiveNodeDescriptor& descriptor)
{
	rapidjson::StringBuffer buffer;
	HiveJsonWriter writer(buffer);

	writer.StartObject();

	__writeKey(writer, "type");
	__writeString(writer, hiveNames::nodeTypeName(descriptor.type));

	__writeKey(writer, "name");
	__writeString(writer, descriptor.name);

	__writeKey(writer, "pokeEnabled");
	writer.Bool(descriptor.pokeEnabled);

	__writeEdges(writer, descriptor);

	switch(descriptor.type)
	{
		case HiveNodeDescriptor::PING:
		case HiveNodeDescriptor::SCENE_ROOT:
			break;

		case HiveNodeDescriptor::TELEPORT:
			__writeDestination(writer, descriptor);
			break;

		case HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT:
			__writeScriptSource(writer, descriptor);
			[[fallthrough]];

		case HiveNodeDescriptor::SCENE_GEOMETRY:
			__writeVertexGroups(writer, descriptor);
			__writeKey(writer, "emitAgentAffectAction");
			writer.Bool(descriptor.emitAgentAffectAction);
			break;

		case HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT:
			__writeScriptSource(writer, descriptor);
			[[fallthrough]];

		case HiveNodeDescriptor::SCENE_TRANSFORM:
			// Left out rather than written as identity, so that a node that was given none reads back as given none.
			if(descriptor.hasTransform)
			{
				__writeKey(writer, "transform");
				__writeDoubleArray(writer, descriptor.transform, 16);
			}

			__writeKey(writer, "emitAgentAffectAction");
			writer.Bool(descriptor.emitAgentAffectAction);
			break;

		case HiveNodeDescriptor::SCENE_TEXT_SCRIPT:
			__writeScriptSource(writer, descriptor);
			__writeTextFields(writer, descriptor);
			break;

		case HiveNodeDescriptor::AGENT:
			__writeAgentFields(writer, descriptor);
			break;

		case HiveNodeDescriptor::TRIGGER:
			__writeScriptSource(writer, descriptor);
			__writeKey(writer, "emitTriggerOnPoke");
			writer.Bool(descriptor.emitTriggerOnPoke);
			break;

		case HiveNodeDescriptor::SCENE_IMAGE:
			__writeImageFields(writer, descriptor);
			break;

		case HiveNodeDescriptor::BUILDER_SCRIPT:
			__writeScriptSource(writer, descriptor);
			__writeKey(writer, "buildOnce");
			writer.Bool(descriptor.buildOnce);
			__writeKey(writer, "built");
			writer.Bool(descriptor.built);
			break;
	}

	writer.EndObject();

	return std::string(buffer.GetString(), buffer.GetSize());
}

std::string JsonHiveWriter::surfaceFragment(const HiveSurfaceDescriptor& descriptor)
{
	rapidjson::StringBuffer buffer;
	HiveJsonWriter writer(buffer);

	writer.StartObject();

	__writeKey(writer, "type");
	__writeString(writer, hiveNames::surfaceTypeName(descriptor.type));

	__writeKey(writer, "name");
	__writeString(writer, descriptor.name);

	if(descriptor.type == HiveSurfaceDescriptor::SCENE_SURFACE)
	{
		__writeKey(writer, "sceneRootNodeName");
		__writeString(writer, descriptor.sceneRootNodeName);

		// Left out when unset, as an empty name would read back as naming a node.
		if(!descriptor.initialFocusNodeName.empty())
		{
			__writeKey(writer, "initialFocusNodeName");
			__writeString(writer, descriptor.initialFocusNodeName);
		}

		__writeKey(writer, "focusViewportFraction");
		writer.Double(descriptor.focusViewportFraction);
	}

	__writeKey(writer, "default");
	writer.Bool(descriptor.isDefault);

	writer.EndObject();

	return std::string(buffer.GetString(), buffer.GetSize());
}

std::string JsonHiveWriter::strobeEmitterFragment(const std::string& nodeName, unsigned periodMs)
{
	return __strobeFragment("nodeName", nodeName, periodMs);
}

std::string JsonHiveWriter::strobeSurfaceFragment(const std::string& surfaceName, unsigned periodMs)
{
	return __strobeFragment("surfaceName", surfaceName, periodMs);
}

std::string JsonHiveWriter::hiveDocument(const HiveSnapshot& snapshot)
{
	// The name is written by the JSON writer, so that it is escaped as any other string is. Everything else is
	// made of fragments, which are each complete JSON values already.
	rapidjson::StringBuffer nameBuffer;
	HiveJsonWriter nameWriter(nameBuffer);

	__writeString(nameWriter, snapshot.name);

	std::string document = "{\"name\":" + std::string(nameBuffer.GetString(), nameBuffer.GetSize());

	document += ",\"nodes\":[";

	for(std::size_t i = 0; i < snapshot.nodes.size(); i++)
	{
		if(i > 0) document += ",";

		document += nodeFragment(snapshot.nodes[i]);
	}

	document += "],\"strobeEmitters\":[";

	for(std::size_t i = 0; i < snapshot.strobeEmitters.size(); i++)
	{
		if(i > 0) document += ",";

		document += strobeEmitterFragment(snapshot.strobeEmitters[i].name, snapshot.strobeEmitters[i].periodMs);
	}

	document += "],\"surfaces\":[";

	for(std::size_t i = 0; i < snapshot.surfaces.size(); i++)
	{
		if(i > 0) document += ",";

		document += surfaceFragment(snapshot.surfaces[i]);
	}

	document += "],\"strobeSurfaces\":[";

	for(std::size_t i = 0; i < snapshot.strobeSurfaces.size(); i++)
	{
		if(i > 0) document += ",";

		document += strobeSurfaceFragment(snapshot.strobeSurfaces[i].name, snapshot.strobeSurfaces[i].periodMs);
	}

	document += "]}";

	return document;
}
