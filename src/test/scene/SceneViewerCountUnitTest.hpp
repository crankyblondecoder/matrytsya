#ifndef SCENE_VIEWER_COUNT_UNIT_TEST_H
#define SCENE_VIEWER_COUNT_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../util/Handle.hpp"
#include "../../graph/GraphHiveSceneSurface.hpp"
#include "../../graph/nodes/SceneRootNode.hpp"

#include <string>
#include <time.h>

namespace
{
	// Viewer tracking needs no hive or population, only a surface to count against.
	Handle<GraphHiveSceneSurface> makeViewerCountSurface()
	{
		return Handle<GraphHiveSceneSurface>(new GraphHiveSceneSurface(Handle<SceneRootNode>(new SceneRootNode())));
	}

	void sleepMs(unsigned ms)
	{
		struct timespec delay;

		delay.tv_sec = ms / 1000;
		delay.tv_nsec = (long) (ms % 1000) * 1000000;

		nanosleep(&delay, 0);
	}
}

TEST(SceneViewerCountUnitTest, CountsDistinctViewersOnce)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 0u);

	surface.getInstance() -> touchViewer("a", 60000);
	surface.getInstance() -> touchViewer("b", 60000);
	surface.getInstance() -> touchViewer("a", 60000);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 2u);
}

TEST(SceneViewerCountUnitTest, RemovedViewerIsNoLongerCounted)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	surface.getInstance() -> touchViewer("a", 60000);
	surface.getInstance() -> touchViewer("b", 60000);
	surface.getInstance() -> removeViewer("a");
	surface.getInstance() -> removeViewer("unknown");

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1u);
}

TEST(SceneViewerCountUnitTest, ViewerExpiresWithoutReportingIn)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	surface.getInstance() -> touchViewer("short", 20);
	surface.getInstance() -> touchViewer("long", 60000);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 2u);

	sleepMs(60);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1u);
}

TEST(SceneViewerCountUnitTest, RefreshingViewerExtendsExpiry)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	surface.getInstance() -> touchViewer("a", 20);
	surface.getInstance() -> touchViewer("a", 60000);

	sleepMs(60);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1u);
}

TEST(SceneViewerCountUnitTest, InvalidViewerIdsAreIgnored)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	surface.getInstance() -> touchViewer("", 60000);
	surface.getInstance() -> touchViewer(std::string(65, 'x'), 60000);
	surface.getInstance() -> touchViewer(std::string(64, 'x'), 60000);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1u);
}

TEST(SceneViewerCountUnitTest, NewViewersRefusedOnceFull)
{
	Handle<GraphHiveSceneSurface> surface = makeViewerCountSurface();

	for(unsigned index = 0; index < 1100; index++) surface.getInstance() -> touchViewer(std::to_string(index), 60000);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1024u);

	// One already counted can still refresh.
	surface.getInstance() -> touchViewer("0", 60000);

	EXPECT_EQ(surface.getInstance() -> getViewerCount(), 1024u);
}

#endif
