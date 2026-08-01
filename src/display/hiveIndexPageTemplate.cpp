#include "hiveIndexPageTemplate.hpp"

// The listing page a GraphHiveCollectionHttpMap serves. One template covers both levels of the index - the
// hives held by a collection and the surface maps mounted for one of those hives - because the two differ
// only in their heading and in what the links point at, both of which are filled in by the map serving it.
// %LINKS% is the whole list body rather than a repeated row, so a page with nothing to offer says so in the
// same place a list would otherwise be, and %BACK_LINK% is left empty by the topmost page, which has nowhere
// above it to go.
const char* const hiveIndexPageTemplate = R"HTMLPAGE(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>%TITLE%</title>
<style>
	html, body { margin: 0; padding: 0; background: #202020; }
	body { color: #ddd; font-family: sans-serif; font-size: 14px; }
	.index { max-width: 640px; margin: 0 auto; padding: 32px 16px; }
	h1 { font-size: 20px; font-weight: normal; color: #fff; margin: 0 0 24px 0; }
	ul { list-style: none; margin: 0; padding: 0; }
	li { margin: 0 0 2px 0; }
	li a { display: block; padding: 10px 12px; background: #2a2a2a; color: #ddd; text-decoration: none; }
	li a:hover { background: #333; color: #fff; }
	.empty { padding: 10px 12px; background: #2a2a2a; color: #888; }
	.back { margin: 24px 0 0 0; }
	.back a { color: #888; text-decoration: none; font-size: 13px; }
	.back a:hover { color: #ddd; }
</style>
</head>
<body>
<div class="index">
<h1>%HEADING%</h1>
<ul>
%LINKS%
</ul>
%BACK_LINK%
</div>
</body>
</html>
)HTMLPAGE";
