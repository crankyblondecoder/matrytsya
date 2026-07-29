#ifndef GRAPH_HIVE_SCENE_SURFACE_HTML_MAP_H
#define GRAPH_HIVE_SCENE_SURFACE_HTML_MAP_H

#include <string>
#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../util/CastHandle.hpp"
#include "../util/EventEmitter.hpp"
#include "../util/Handle.hpp"
#include "../graph/GraphHiveSurfaceListener.hpp"
#include "../thread/Thread.hpp"
#include "../thread/ThreadCondition.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/EventListener.hpp"
#include "GraphHiveSurfaceHttpMap.hpp"

class GraphHiveSceneSurface;
class GraphHiveSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Maps a GraphHiveSceneSurface onto an HTML interface so that it can be viewed in a browser, in whatever way a
 * subclass chooses to draw it. Everything a browser needs of the bound surface whichever way it is drawn is held
 * here: the revision browsers poll to detect a change, the pokes made when part of the surface is interacted with,
 * and the chat window carried on the page. What is left to a subclass is the page itself and the data it draws
 * the surface from.
 * This map binds to a single scene surface for its whole lifetime and listens for that surface's changed event
 * to know when browsers viewing its page should pick up new data, without needing to be reloaded.
 * A chat is answered by a model, which takes far longer than an HTTP request should be held open for, so prompts
 * are queued here and serviced on this map's own thread while the browser polls for the reply. That keeps the
 * thread serving the page free to answer the viewer's revision and surface data requests, so the page carries on
 * drawing while a chat is being answered.
 */
class GraphHiveSceneSurfaceHtmlMap : public GraphHiveSurfaceHttpMap, private EventListener<GraphHiveSurfaceListener>,
	private Thread
{
    public:

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Scene surface this map binds to for its whole lifetime. This map keeps its own
         *        reference to it (see Handle), released once this map is destroyed.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSceneSurfaceHtmlMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface, std::string path);

        /**
         * Set how often browsers viewing this map's page poll to check for changes.
         * @param pollIntervalMs Poll interval in milliseconds. Defaults to 1000.
         */
        void setPollInterval(unsigned pollIntervalMs);

        /**
         * Set the capability required of the model that answers chats made through this map's page.
         * @note Held here rather than asked of the browser, so that which model a page may reach is decided by
         *       whoever mounted the map and not by whoever loads it.
         * @param capability Capability to require. Defaults to MEDIUM.
         */
        void setChatCapability(AgenticHarness::Capability capability);

    protected:

        // Required by ref counting.
        virtual ~GraphHiveSceneSurfaceHtmlMap();

        /**
         * Get the scene surface this map is bound to.
         */
        GraphHiveSceneSurface& _getSceneSurface();

        /**
         * Get the path this map is mounted at with any trailing separator taken off, which is what this map's
         * endpoints hang from.
         */
        std::string _getBasePath();

        /**
         * Render a page from a template, filling in the placeholders every page served by this map carries.
         * Those are %TITLE%, which becomes the bound surface's name, %POLL_INTERVAL_MS%, which becomes the
         * interval the page is to poll this map's revision endpoint at, and %CHAT_STYLE%, %CHAT_MARKUP% and
         * %CHAT_SCRIPT%, which become the chat window's style rules, markup and script (see chatWindowTemplate.hpp
         * for where in a page each of the three belongs). Each may appear any number of times.
         * @note A template is free to leave the chat placeholders out, which is all it takes to serve a page
         *       without a chat window on it; the endpoints behind one are answered either way.
         * @param pageTemplate Template to render.
         * @param response Response to populate with the rendered page.
         */
        void _renderPageTemplate(const std::string& pageTemplate, HttpResponse& response);

        /**
         * Serve a data request that is specific to how a subclass draws the bound surface, i.e. one none of the
         * endpoints common to every page served by this map answered.
         * @param request The incoming data request.
         * @param response Response to populate with the requested data.
         */
        virtual void _serveMapData(HttpRequest& request, HttpResponse& response) = 0;

        void _serveData(HttpRequest& request, HttpResponse& response) final;

    private:

        /// State a chat prompt accepted from a browser is in.
        enum class ChatState
        {
            /// Queued, waiting for this map's chat thread to pick it up.
            PENDING,
            /// Currently with the model.
            SERVICING,
            /// The model answered it; the reply is held with it.
            ANSWERED,
            /// It could not be serviced; why is held with it.
            FAILED
        };

        /**
         * A chat prompt accepted from a browser, and the reply given to it once there is one.
         */
        struct ChatMessage
        {
            /// Id this map named the prompt by when it accepted it, which the browser polls for the reply with.
            unsigned id;

            /// Context the chat is held in. Meaningless until the surface has answered when newContext is true.
            unsigned contextId;

            /// Whether the chat starts a fresh conversation rather than continuing the one contextId names.
            bool newContext;

            /// Text of the prompt.
            std::string prompt;

            /// What the model said. Only meaningful once the state is ANSWERED.
            std::string reply;

            /// Why the chat could not be serviced. Only meaningful once the state is FAILED.
            std::string error;

            /// Where the prompt has got to.
            ChatState state;
        };

        // Disable copying.
        GraphHiveSceneSurfaceHtmlMap(const GraphHiveSceneSurfaceHtmlMap& copyFrom);
        GraphHiveSceneSurfaceHtmlMap& operator= (const GraphHiveSceneSurfaceHtmlMap& copyFrom);

        /**
         * Serve the lightweight revision check browsers poll to detect a change to the bound surface.
         * @param response Response to populate.
         */
        void __serveRevision(HttpResponse& response);

        /**
         * Serve a poke request made by the rendered page when a chunk of the surface is clicked on or hovered over.
         * @param request The incoming poke request. Must carry both a "nodeId" and a "chunkId" query parameter,
         *        which together identify the poked chunk. May carry a "type" query parameter of "hoverEnter" or
         *        "hoverLeave" to raise a HOVER_ENTER or HOVER_LEAVE poke instead of the default HIT.
         * @param response Response to populate.
         */
        void __servePoke(HttpRequest& request, HttpResponse& response);

        /**
         * Accept a chat prompt made by the rendered page, queueing it for this map's chat thread to put to the
         * bound surface. Answers straight away with the id the reply is to be polled for, rather than holding the
         * request open for however long the model takes.
         * @param request The incoming chat request. Its body must be
         *        {"prompt":"...","contextId":N}, where contextId names the conversation to continue and is left
         *        out to start a fresh one. A conversation's id is only ever handed out by __serveChatMessage()
         *        below, once the chat that started it has been answered.
         * @param response Response to populate.
         */
        void __serveChat(HttpRequest& request, HttpResponse& response);

        /**
         * Serve the state of a chat prompt this map accepted earlier, which is how the page collects the reply.
         * @param request The incoming request. Must carry a "messageId" query parameter, as handed out by
         *        __serveChat().
         * @param response Response to populate.
         */
        void __serveChatMessage(HttpRequest& request, HttpResponse& response);

        /**
         * Serve the list of conversations currently held through the bound surface, so the page can offer them
         * to be carried on with.
         * @param response Response to populate.
         */
        void __serveChatContexts(HttpResponse& response);

        /**
         * Serve a request from the page to discard a conversation held through the bound surface.
         * @param request The incoming request. Must carry a "contextId" query parameter.
         * @param response Response to populate.
         */
        void __serveChatContextRemove(HttpRequest& request, HttpResponse& response);

        /**
         * Start this map's chat thread, if it is not already running.
         * @note Done on the first chat rather than on construction, so a page that is only ever looked at never
         *       costs a thread.
         * @note A thread is one shot, so a start that failed is never tried again.
         * @returns True if there is a chat thread running to service queued prompts, false if one could not be
         *          started. Never throws: an HTTP request is served from the HTTP server's own event loop, which
         *          has nowhere to report an exception to.
         */
        bool __startChatThread();

        /**
         * Queue a chat prompt for this map's chat thread.
         * @param prompt Text of the prompt.
         * @param newContext Whether to start a fresh conversation rather than continue the one contextId names.
         * @param contextId Conversation to continue. Ignored when newContext is true.
         * @returns The id the queued prompt is named by.
         */
        unsigned __queueChatMessage(std::string prompt, bool newContext, unsigned contextId);

        /**
         * Take the next queued chat prompt for this map's chat thread to put to the bound surface, waiting a
         * short while for one when the queue is empty.
         * @param message Populated with a copy of the taken prompt.
         * @returns True if a prompt was taken, false if none was queued.
         */
        bool __takeNextChatMessage(ChatMessage& message);

        /**
         * Record the outcome of a chat prompt, so the browser polling for it collects it.
         * @param messageId Id of the prompt, as given out by __queueChatMessage().
         * @param contextId Conversation the chat was serviced in, as reported by the surface.
         * @param reply What the model said. Ignored when error is not empty.
         * @param error Why the chat could not be serviced, or empty if it was.
         */
        void __completeChatMessage(unsigned messageId, unsigned contextId, std::string reply, std::string error);

        virtual void threadEntry() override;

        virtual void _quitRequested() override;

        virtual void hiveSurfaceChanged(EventEmitter<GraphHiveSurfaceListener>& emitter) override;

        virtual void populateEventListenerHandle(CastHandle<GraphHiveSurfaceListener>& handle) override;

        /// Scene surface this map is bound to for its whole lifetime.
        Handle<GraphHiveSceneSurface> _sceneSurface;

        /// Bumped every time the bound surface reports a change, so polling browsers can detect it.
        unsigned _revision = 0;

        /// How often browsers viewing this map's page poll to check for changes, in milliseconds.
        unsigned _pollIntervalMs = 1000;

        /// Capability required of the model that answers chats made through this map's page.
        AgenticHarness::Capability _chatCapability = AgenticHarness::Capability::MEDIUM;

        /// Whether starting this map's chat thread has been attempted, which only happens once a first chat is
        /// made so that a page that is only ever looked at never costs a thread.
        bool _chatThreadStarted = false;

        /// Whether that attempt produced a running thread. Told apart from the attempt itself so that a start
        /// which failed is neither retried against a thread that can only be started once, nor stopped in the
        /// destructor as though it were running.
        bool _chatThreadRunning = false;

        /// Guards _revision, _pollIntervalMs, _chatCapability, _chatThreadStarted and _chatThreadRunning, all of
        /// which are written and read across the thread serving HTTP requests, the chat thread and the surface's
        /// own threads.
        ThreadMutex _lock;

        /// The chat prompts accepted from browsers, oldest first, in every state. Guarded by _chatMessagesCond,
        /// which is signalled whenever one is queued so the chat thread picks it up without waiting out its poll.
        std::vector<ChatMessage> _chatMessages;

        /// Guards _chatMessages and _nextChatMessageId, and wakes the chat thread when a prompt is queued.
        ThreadCondition _chatMessagesCond;

        /// Counter used to name each accepted chat prompt.
        unsigned _nextChatMessageId = 1;
};

#endif
