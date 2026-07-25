#ifndef CHAT_WINDOW_TEMPLATE_H
#define CHAT_WINDOW_TEMPLATE_H

/// Style rules of the chat window, for placing inside a page's <style> element.
extern const char* const chatWindowStyle;

/// Markup of the chat window and the button that shows it, for placing inside a page's <body> element.
extern const char* const chatWindowMarkup;

/// Script driving the chat window, for placing inside a page's <script> element. Self contained, so it may go
/// wherever in the page suits and never has to be woven into the script that draws the page itself.
extern const char* const chatWindowScript;

#endif
