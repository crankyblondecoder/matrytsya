#ifndef GRAPH_HIVE_SURFACE_H
#define GRAPH_HIVE_SURFACE_H

#include <string>
#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../util/EventEmitter.hpp"
#include "../util/RefCounted.hpp"
#include "../util/Handle.hpp"
#include "GraphHive.hpp"
#include "GraphHiveSurfaceListener.hpp"
#include "GraphNamed.hpp"
#include "GraphPoke.hpp"

class ModelContext;

/**
 * Represents a "surface" that a hive can interact with, either for display or input.
 * It essentially provides a layer of abstraction for various display types but keeps the interface that an action
 * has to use consistent
 * A surface is said to be "populating" when it is being constructed so it can be interacted with.
 */
class GraphHiveSurface : public RefCounted, public GraphNamed, public EventEmitter<GraphHiveSurfaceListener>
{
	public:

		/// Identifies the concrete subclass of a surface, so callers can find a specific kind without an RTTI cast.
		enum class Type
		{
			/// A GraphHiveSceneSurface.
			SCENE_SURFACE
		};

		/**
		 * Names a conversation held through a surface and says what it is about, so that one can be picked
		 * out of several without having to read back what was said in each.
		 */
		struct ChatContext
		{
			/// Id the conversation is named by, as given out by chat().
			unsigned id;

			/// What the conversation is about.
			std::string description;
		};

		/**
		 * @param type Concrete type of this surface, as reported by getType().
		 */
		GraphHiveSurface(Type type);

		/**
		 * Get the concrete type of this surface.
		 */
		Type getType();

		/**
		 * Get whether this surface is the default surface of its kind within its hive.
		 */
		bool getDefault();

		/**
		 * Set whether this surface is the default surface of its kind within its hive.
		 * @param isDefault Whether this surface is default.
		 */
		void setDefault(bool isDefault);

		/**
		 * Set the hive this surface is bound to.
		 * @param hive Hive this surface is to be bound to. Must be a valid handle.
		 */
		void setHive(Handle<GraphHive> hive);

		/**
		 * Activate this surface.
		 * This must be done for the surface to start interacting with the hive.
		 */
		virtual void activate() = 0;

		/**
		 * Request this surface to go into population mode (start populate pass).
		 * @param populateVersion The version to assign to this populate pass.
		 * @returns True if could go into population mode, false otherwise. It will return false if already in
		 *          population mode.
		 */
		bool populateStart(unsigned populateVersion);

		/**
		 * Request this surface to go out of population mode.
		 */
		void populateEnd();

		/**
		 * Get whether this surface is in population mode.
		 */
		bool isPopulating();

		/**
		 * Get the version assigned to the last populate pass.
		 */
		unsigned getPopulateVersion();

		/**
		 * Clean up and dereference this surface.
		 */
		virtual void close() final;

		/**
		 * Poke this surface.
		 * @param nodeId The id of the node that is to be poked.
		 * @param poke Poke to apply.
		 */
		virtual void poke(unsigned nodeId, GraphPoke poke);

		/**
		 * Chat with the model the hive this surface is bound to assigns to its chat role.
		 * @note A conversation is held in a context, which this surface keeps until it is removed with
		 *       removeChatContext() or the surface itself goes, so that a prompt made in a context is
		 *       answered in the light of everything already said in it.
		 * @note A context services one chat at a time, so a chat made in a context that is still answering
		 *       an earlier prompt is refused rather than interleaved with it.
		 * @note The capability always chooses the model, but the system prompts and tools of a context are
		 *       fixed when it is created, so raising it on a continued conversation changes which model
		 *       answers without changing what it is told or what it may call.
		 * @param prompt Text of the prompt to send to the model.
		 * @param capability Capability required of the model. Matched exactly against assigned models.
		 * @param newContext True to start a fresh conversation, in which case contextId is ignored on the
		 *        way in. False to continue the conversation held in the context contextId names.
		 * @param contextId On entry, the id of the context to continue when newContext is false. On
		 *        return, the id of the context the chat was serviced in, which is a newly created one when
		 *        newContext is true and the id passed in otherwise. Pass it back to keep chatting in the
		 *        same conversation. Ids come from the contexts themselves and are unique across the
		 *        process, so one is never handed out twice, whatever surface it came from.
		 * @returns The reply the model gave to the prompt.
		 * @throw GraphException When this surface is not bound to a hive, when newContext is false and
		 *        contextId does not name a context of this surface, or when the hive has no agentic
		 *        harness set.
		 * @throw AgentException When no prompt text was supplied, when no model of the requested capability is
		 *        assigned to the chat role, or when the context is already servicing a chat.
		 */
		std::string chat(std::string prompt, AgenticHarness::Capability capability, bool newContext,
			unsigned& contextId);

		/**
		 * Remove a conversation held through this surface, discarding its context and everything said in it.
		 * @note The id is not handed out again once removed, so a caller holding a stale one is told it
		 *       names nothing rather than being given a conversation it never asked for.
		 * @note A chat still being serviced in the context keeps it alive until it is answered, so this
		 *       neither waits on nor cancels one. It only means no further chat can be made in it.
		 * @param contextId Id of the context to remove, as given out by chat().
		 * @throw GraphException When contextId does not name a context of this surface, which includes one
		 *        already removed.
		 */
		void removeChatContext(unsigned contextId);

		/**
		 * Get the id and description of every conversation currently held through this surface, in the
		 * order they were started.
		 * @note A snapshot taken when called. A conversation started or removed after it returns is not
		 *       reflected in what it returned.
		 */
		std::vector<ChatContext> getChatContexts();

		/**
		 * Strobe this surface.
		 * This causes it to update/regenerate if required.
		 */
		virtual void strobe() = 0;

	protected:

		// Required by ref counting.
		virtual ~GraphHiveSurface();

		/**
		 * Subclass hook to indicate it should close.
		 */
		virtual void _close() = 0;

		/**
		 * Subclass hook to inform that population has started.
		 */
		virtual void _populateStart() = 0;

		/**
		 * Subclass hook to inform that population has ended.
		 */
		virtual void _populateEnd() = 0;

		/**
		 * Emit the surface changed event.
		 */
		void _emitSurfaceChanged();

	private:

		// Disable copying.
		GraphHiveSurface(const GraphHiveSurface& copyFrom);
		GraphHiveSurface& operator= (const GraphHiveSurface& copyFrom);

		/**
		 * Find a context of a conversation held through this surface by its id.
		 * @param contextId Id of the context to find.
		 * @returns Handle to the context. Invalid handle if this surface holds no context with that id.
		 */
		Handle<ModelContext> __findChatContext(unsigned contextId);

		/// Concrete type of this surface.
		Type _type;

		/// Whether this surface is the default surface of its kind within its hive.
		bool _default = false;

		/// Whether this surface is currently in population mode.
		bool _populating = false;

		/// Generic lock.
		ThreadMutex _lock;

		/// Hive this surface is bound to.
		Handle<GraphHive> _hive;

		/// Contexts of the conversations held through this surface, in the order they were started. Each
		/// carries the id it is named by, so position in here means nothing to a caller.
		std::vector<Handle<ModelContext>> _chatContexts;

		/// The version assigned to that last populate pass.
		unsigned _populateVersion = 0;
};

#endif
