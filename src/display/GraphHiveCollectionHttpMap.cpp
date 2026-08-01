#include "GraphHiveCollectionHttpMap.hpp"

#include <cstddef>

#include "../graph/GraphHiveCollection.hpp"
#include "../thread/thread.hpp"
#include "GraphHiveSurfaceMap.hpp"
#include "hiveIndexPageTemplate.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpServerBase.hpp"

namespace
{
	/// Title the top page carries, which is also the label of the link back to it from a hive's page.
	const char* const _INDEX_TITLE = "Hives";

	// Escapes a string so that it can be put into the page as text or into an attribute value without any part
	// of it being read as markup. A hive or surface name is whatever a hive definition file said it was, so
	// nothing here can assume it is safe to write out as it stands.
	std::string htmlEscape(const std::string& value)
	{
		std::string result;

		for(char character : value)
		{
			switch(character)
			{
				case '&':  result += "&amp;"; break;
				case '<':  result += "&lt;"; break;
				case '>':  result += "&gt;"; break;
				case '"':  result += "&quot;"; break;
				case '\'': result += "&#39;"; break;

				default: result += character; break;
			}
		}

		return result;
	}

	// Replaces every occurrence of a page template's placeholder. Searching on from the end of what was just put
	// in rather than from where it went keeps a value that itself contains the placeholder from being expanded
	// again for ever.
	void replacePlaceholder(std::string& page, const std::string& placeholder, const std::string& value)
	{
		std::size_t position = page.find(placeholder);

		while(position != std::string::npos)
		{
			page.replace(position, placeholder.size(), value);

			position = page.find(placeholder, position + value.size());
		}
	}

	// Builds one link of a listing.
	std::string listLink(const std::string& path, const std::string& label)
	{
		return "<li><a href=\"" + htmlEscape(path) + "\">" + htmlEscape(label) + "</a></li>\n";
	}

	// Renders a listing page and puts it into a response.
	void renderPage(const std::string& title, const std::string& heading, const std::string& links,
		const std::string& backLink, HttpResponse& response)
	{
		std::string page = hiveIndexPageTemplate;

		replacePlaceholder(page, "%TITLE%", title);
		replacePlaceholder(page, "%HEADING%", heading);
		replacePlaceholder(page, "%LINKS%", links);
		replacePlaceholder(page, "%BACK_LINK%", backLink);

		response.setContentType("text/html");
		response.setBody(page);
	}
}

GraphHiveCollectionHttpMap::GraphHiveCollectionHttpMap(HttpServerBase& httpServer, GraphHiveCollection& collection,
	std::string path) :
	_httpServer{httpServer}, _collection{collection}, _path{path}
{
	_httpServer.addHandler(_path, this);
}

GraphHiveCollectionHttpMap::~GraphHiveCollectionHttpMap()
{
	_httpServer.removeHandler(_path);
}

std::string GraphHiveCollectionHttpMap::getPath()
{
	return _path;
}

void GraphHiveCollectionHttpMap::addSurfaceMap(std::string hiveName, GraphHiveSurfaceMap& surfaceMap)
{
	SurfaceEntry entry;

	// Taken outside the block below, as both reach back into the map and the surface it holds.
	entry.surfaceName = surfaceMap.getSurfaceName();
	entry.path = surfaceMap.getPath();

	{ SYNC(_lock)

		bool added = false;

		for(HiveEntry& hive : _hives)
		{
			if(hive.hiveName == hiveName)
			{
				hive.surfaces.push_back(entry);

				added = true;
				break;
			}
		}

		if(!added)
		{
			HiveEntry hive;

			hive.hiveName = hiveName;
			hive.surfaces.push_back(entry);

			_hives.push_back(hive);
		}
	}
}

void GraphHiveCollectionHttpMap::handleRequest(HttpRequest& request, HttpResponse& response)
{
	std::string requestPath = request.getPath();

	if(requestPath == _path)
	{
		__serveHiveIndex(response);
		return;
	}

	// A hive is only given a page if the collection still holds it, so a link followed after the hive went
	// answers as an unknown path rather than as an empty hive.
	for(const std::string& hiveName : _collection.getHiveNames())
	{
		std::string hivePath = __hivePath(hiveName);

		// Also matched without the trailing separator, so that a hand typed address reaches the same page the
		// index links to.
		if(requestPath == hivePath || requestPath + "/" == hivePath)
		{
			__serveHivePage(hiveName, response);
			return;
		}
	}

	// Mounted at the server root, this map is what anything not claimed by a surface map falls through to, so
	// it has to answer for an unknown path exactly as the server does for one nothing is mounted under.
	response.setStatus(404);
	response.setContentType("text/plain");
	response.setBody("Not Found");
}

void GraphHiveCollectionHttpMap::__serveHiveIndex(HttpResponse& response)
{
	std::string links;

	for(const std::string& hiveName : _collection.getHiveNames())
	{
		links += listLink(__hivePath(hiveName), hiveName);
	}

	if(links.empty()) links = "<li class=\"empty\">No hives are loaded.</li>\n";

	renderPage(_INDEX_TITLE, _INDEX_TITLE, links, "", response);
}

void GraphHiveCollectionHttpMap::__serveHivePage(const std::string& hiveName, HttpResponse& response)
{
	std::string links;

	for(const SurfaceEntry& entry : __getSurfaceEntries(hiveName))
	{
		links += listLink(entry.path, entry.surfaceName);
	}

	if(links.empty()) links = "<li class=\"empty\">No surface maps are mounted for this hive.</li>\n";

	std::string backLink = "<p class=\"back\"><a href=\"" + htmlEscape(_path) + "\">&larr; " + _INDEX_TITLE +
		"</a></p>";

	renderPage(htmlEscape(hiveName), htmlEscape(hiveName), links, backLink, response);
}

std::string GraphHiveCollectionHttpMap::__hivePath(const std::string& hiveName)
{
	return _path + hiveName + "/";
}

std::vector<GraphHiveCollectionHttpMap::SurfaceEntry> GraphHiveCollectionHttpMap::__getSurfaceEntries(
	const std::string& hiveName)
{
	std::vector<SurfaceEntry> entries;

	{ SYNC(_lock)

		for(const HiveEntry& hive : _hives)
		{
			if(hive.hiveName == hiveName)
			{
				entries = hive.surfaces;
				break;
			}
		}
	}

	return entries;
}
