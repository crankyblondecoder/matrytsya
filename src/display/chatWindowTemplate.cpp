#include "chatWindowTemplate.hpp"

// The chat window carried on every page served by GraphHiveSceneSurfaceHtmlMap, kept here rather than in any one
// page so that each page need only say where the three pieces below go. A floating panel over whatever the page
// draws, dragged around by its header, onto the chat interface of the surface the page's map is bound to. A prompt
// takes as long as the model takes, which is far too long to hold an HTTP request open for, so the server accepts a
// prompt and answers with an id to poll for the reply. That leaves whatever polling the page does of its own free
// to carry on drawing while a chat is being answered.

const char* const chatWindowStyle = R"CHATSTYLE(
	#chatToggle { position: absolute; right: 12px; bottom: 12px; padding: 6px 12px; color: #ddd;
		background: #303030; border: 1px solid #4a4a4a; border-radius: 4px; font-family: sans-serif;
		font-size: 13px; cursor: pointer; }
	#chatToggle:hover { background: #3a3a3a; }
	#chatWindow { position: absolute; display: flex; flex-direction: column; width: 340px; height: 430px;
		background: #282828; border: 1px solid #4a4a4a; border-radius: 5px; box-shadow: 0 6px 18px rgba(0, 0, 0, 0.5);
		color: #ddd; font-family: sans-serif; font-size: 13px; overflow: hidden; }
	#chatWindow[hidden] { display: none; }
	#chatHeader { display: flex; align-items: center; padding: 6px 8px; background: #333333; cursor: move;
		border-bottom: 1px solid #4a4a4a; user-select: none; -webkit-user-select: none; }
	#chatHeader span { flex: 1; }
	#chatBar { display: flex; padding: 6px 8px; border-bottom: 1px solid #383838; }
	#chatBar > * { margin-right: 4px; }
	#chatBar > *:last-child { margin-right: 0; }
	#chatContextSelect { flex: 1; min-width: 0; padding: 2px; color: #ddd; background: #303030;
		border: 1px solid #4a4a4a; border-radius: 3px; font-family: inherit; font-size: 12px; }
	#chatLog { flex: 1; padding: 8px; overflow-y: auto; }
	#chatInputRow { display: flex; padding: 8px; border-top: 1px solid #383838; }
	#chatPromptInput { flex: 1; min-width: 0; margin-right: 4px; padding: 4px; color: #ddd; background: #303030;
		border: 1px solid #4a4a4a; border-radius: 3px; font-family: inherit; font-size: 12px; resize: none; }
	#chatWindow button { padding: 3px 8px; color: #ddd; background: #383838; border: 1px solid #4a4a4a;
		border-radius: 3px; font-family: inherit; font-size: 12px; cursor: pointer; }
	#chatWindow button:hover:enabled { background: #444444; }
	#chatWindow button:disabled { color: #777; cursor: default; }
	.chatEntry { margin-bottom: 8px; line-height: 1.35; white-space: pre-wrap; overflow-wrap: break-word; }
	.chatEntry .chatWho { display: block; margin-bottom: 2px; color: #888; font-size: 11px; }
	.chatPrompt .chatBody { color: #cfe3ff; }
	.chatError .chatBody { color: #ff9a8a; }
	.chatNote { color: #888; font-style: italic; }
)CHATSTYLE";

const char* const chatWindowMarkup = R"CHATMARKUP(
<button id="chatToggle">Chat</button>
<div id="chatWindow" hidden>
	<div id="chatHeader"><span>Chat</span><button id="chatClose" title="Close">X</button></div>
	<div id="chatBar">
		<select id="chatContextSelect" title="Conversation"></select>
		<button id="chatNew" title="Start a new conversation">New</button>
		<button id="chatRemove" title="Discard this conversation">Discard</button>
	</div>
	<div id="chatLog"></div>
	<div id="chatInputRow">
		<textarea id="chatPromptInput" rows="2" placeholder="Type something..."></textarea>
		<button id="chatSend">Send</button>
	</div>
</div>
)CHATMARKUP";

const char* const chatWindowScript = R"CHATSCRIPT(
(function() {
	'use strict';

	var chatBase = window.location.pathname.replace(/\/+$/, '') + '/chat';
	var chatMessageUrl = chatBase + '/message';
	var chatContextsUrl = chatBase + '/contexts';
	var chatRemoveContextUrl = chatBase + '/removeContext';

	var CHAT_POLL_INTERVAL_MS = 500;

	var chatToggle = document.getElementById('chatToggle');
	var chatWindow = document.getElementById('chatWindow');
	var chatHeader = document.getElementById('chatHeader');
	var chatClose = document.getElementById('chatClose');
	var chatContextSelect = document.getElementById('chatContextSelect');
	var chatNew = document.getElementById('chatNew');
	var chatRemove = document.getElementById('chatRemove');
	var chatLog = document.getElementById('chatLog');
	var chatPromptInput = document.getElementById('chatPromptInput');
	var chatSend = document.getElementById('chatSend');

	// Id of the conversation on show, or null when the next prompt is to start a fresh one.
	var chatContextId = null;

	// What was said in each conversation this page has taken part in, keyed by conversation id, plus the one for
	// a conversation not yet started (which has no id to key it by until its first prompt is answered). The
	// server holds the conversations themselves and this is only what to show of them, so one started before this
	// page was loaded shows as empty until something further is said in it.
	var chatTranscripts = {};
	var chatNewTranscript = [];

	// Id of the prompt currently with the server, or null. Only ever one at a time, since a conversation refuses
	// a prompt made while it is still answering an earlier one.
	var chatPendingMessageId = null;

	function chatTranscript()
	{
		if(chatContextId === null) return chatNewTranscript;

		if(!chatTranscripts[chatContextId]) chatTranscripts[chatContextId] = [];

		return chatTranscripts[chatContextId];
	}

	function renderChatLog()
	{
		var transcript = chatTranscript();

		chatLog.textContent = '';

		if(transcript.length === 0)
		{
			var note = document.createElement('div');

			note.className = 'chatEntry chatNote';
			note.textContent = (chatContextId === null) ?
				'Nothing said yet. Ask something to start a conversation.' :
				'This conversation was not started on this page, so what was already said in it is not shown here.';

			chatLog.appendChild(note);
		}

		for(var i = 0; i < transcript.length; i++)
		{
			var entry = transcript[i];

			var element = document.createElement('div');

			element.className = 'chatEntry ' + entry.kind;

			var who = document.createElement('span');

			who.className = 'chatWho';
			who.textContent = entry.who;

			var body = document.createElement('span');

			body.className = 'chatBody';

			// textContent rather than innerHTML throughout: a reply is whatever the model chose to say, and must
			// never be treated as markup by this page.
			body.textContent = entry.text;

			element.appendChild(who);
			element.appendChild(body);

			chatLog.appendChild(element);
		}

		if(chatPendingMessageId !== null)
		{
			var pending = document.createElement('div');

			pending.className = 'chatEntry chatNote';
			pending.textContent = 'Thinking...';

			chatLog.appendChild(pending);
		}

		chatLog.scrollTop = chatLog.scrollHeight;
	}

	function addChatEntry(kind, who, text)
	{
		chatTranscript().push({ kind: kind, who: who, text: text });

		renderChatLog();
	}

	// Everything that would change which conversation a reply belongs to is shut off while a prompt is with the
	// server, so the reply can only ever land in the transcript the prompt was made from.
	function setChatBusy(busy)
	{
		chatSend.disabled = busy;
		chatNew.disabled = busy;
		chatContextSelect.disabled = busy;
		chatRemove.disabled = busy || chatContextId === null;

		renderChatLog();
	}

	function startNewChatConversation()
	{
		chatContextId = null;
		chatNewTranscript = [];
		chatContextSelect.value = '';

		setChatBusy(false);
	}

	function refreshChatContexts()
	{
		fetch(chatContextsUrl).then(function(res) { return res.json(); }).then(function(data)
		{
			var contexts = data.contexts || [];

			chatContextSelect.textContent = '';

			var newOption = document.createElement('option');

			newOption.value = '';
			newOption.textContent = 'New conversation';

			chatContextSelect.appendChild(newOption);

			var onShowStillHeld = false;

			for(var i = 0; i < contexts.length; i++)
			{
				var option = document.createElement('option');

				option.value = String(contexts[i].id);

				// A conversation describes itself by what was first asked of it, which is empty only for one
				// whose first prompt hasn't been answered yet.
				option.textContent = contexts[i].description || ('Conversation ' + contexts[i].id);

				chatContextSelect.appendChild(option);

				if(contexts[i].id === chatContextId) onShowStillHeld = true;
			}

			// A conversation the surface no longer holds (discarded from another page, or from a surface that has
			// since been replaced) can't be carried on, so fall back to starting a fresh one rather than leaving a
			// selection that would be refused.
			if(chatContextId !== null && !onShowStillHeld) startNewChatConversation();

			chatContextSelect.value = (chatContextId === null) ? '' : String(chatContextId);

		}).catch(function(err)
		{
			// Nothing to do but leave the list as it was; the next refresh will pick it up.
		});
	}

	// Reads a JSON response along with whether it was an error, so a refusal can be reported by what the server
	// said rather than by its status code alone.
	function chatJson(res)
	{
		return res.json().then(function(data) { return { ok: res.ok, data: data }; });
	}

	function chatError(result, fallback)
	{
		return new Error((result.data && result.data.error) ? result.data.error : fallback);
	}

	function pollChatMessage()
	{
		if(chatPendingMessageId === null) return;

		fetch(chatMessageUrl + '?messageId=' + chatPendingMessageId).then(chatJson).then(function(result)
		{
			if(!result.ok) throw chatError(result, 'The server no longer knows about that prompt.');

			if(result.data.state === 'pending')
			{
				setTimeout(pollChatMessage, CHAT_POLL_INTERVAL_MS);
				return;
			}

			chatPendingMessageId = null;

			if(result.data.state === 'answered')
			{
				// A prompt that started a conversation is answered with the id it was given, which is what every
				// prompt after it has to be sent with to stay in the same conversation. Adopted before the reply
				// is added so that the reply lands in that conversation's transcript.
				if(chatContextId === null && typeof result.data.contextId === 'number')
				{
					chatContextId = result.data.contextId;
					chatTranscripts[chatContextId] = chatNewTranscript;
					chatNewTranscript = [];
				}

				addChatEntry('chatReply', 'Model', result.data.reply);

				refreshChatContexts();
			}
			else
			{
				addChatEntry('chatError', 'Error', result.data.error || 'The chat could not be serviced.');
			}

			setChatBusy(false);

		}).catch(function(err)
		{
			chatPendingMessageId = null;

			addChatEntry('chatError', 'Error', String(err.message || err));

			setChatBusy(false);
		});
	}

	function sendChatPrompt()
	{
		var prompt = chatPromptInput.value.trim();

		if(prompt === '' || chatPendingMessageId !== null) return;

		var body = { prompt: prompt };

		// Leaving the conversation id out of the body is what asks for a fresh conversation to be started.
		if(chatContextId !== null) body.contextId = chatContextId;

		chatPromptInput.value = '';

		addChatEntry('chatPrompt', 'You', prompt);

		setChatBusy(true);

		fetch(chatBase, {

			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(body)

		}).then(chatJson).then(function(result)
		{
			if(!result.ok || typeof result.data.messageId !== 'number')
			{
				throw chatError(result, 'The prompt was refused.');
			}

			chatPendingMessageId = result.data.messageId;

			renderChatLog();

			setTimeout(pollChatMessage, CHAT_POLL_INTERVAL_MS);

		}).catch(function(err)
		{
			addChatEntry('chatError', 'Error', String(err.message || err));

			setChatBusy(false);
		});
	}

	chatSend.addEventListener('click', sendChatPrompt);

	chatPromptInput.addEventListener('keydown', function(e)
	{
		// Enter sends, shift-enter is a line break, as everywhere else that has a prompt box.
		if(e.key !== 'Enter' || e.shiftKey) return;

		e.preventDefault();

		sendChatPrompt();
	});

	chatNew.addEventListener('click', startNewChatConversation);

	chatContextSelect.addEventListener('change', function()
	{
		chatContextId = (chatContextSelect.value === '') ? null : parseInt(chatContextSelect.value, 10);

		if(chatContextId === null) chatNewTranscript = [];

		setChatBusy(false);
	});

	chatRemove.addEventListener('click', function()
	{
		if(chatContextId === null) return;

		var removedContextId = chatContextId;

		fetch(chatRemoveContextUrl + '?contextId=' + removedContextId, { method: 'POST' }).then(chatJson)
			.then(function(result)
		{
			if(!result.ok) throw chatError(result, 'The conversation could not be discarded.');

			delete chatTranscripts[removedContextId];

			startNewChatConversation();
			refreshChatContexts();

		}).catch(function(err)
		{
			addChatEntry('chatError', 'Error', String(err.message || err));
		});
	});

	// -- Chat window placement --
	//
	// Dragged around by its header. Whatever the page draws binds its own pointer handlers to the element it draws
	// into, so those never see a drag that started on this window, and the window's position is clamped to the
	// viewport so it can't be dropped somewhere it could no longer be grabbed from.

	var chatDragging = false;
	var chatDragOffsetX = 0, chatDragOffsetY = 0;

	function placeChatWindow(left, top)
	{
		var maxLeft = Math.max(0, window.innerWidth - chatWindow.offsetWidth);
		var maxTop = Math.max(0, window.innerHeight - chatWindow.offsetHeight);

		chatWindow.style.left = Math.max(0, Math.min(maxLeft, left)) + 'px';
		chatWindow.style.top = Math.max(0, Math.min(maxTop, top)) + 'px';
	}

	chatHeader.addEventListener('mousedown', function(e)
	{
		if(e.button !== 0) return;

		var rect = chatWindow.getBoundingClientRect();

		chatDragging = true;
		chatDragOffsetX = e.clientX - rect.left;
		chatDragOffsetY = e.clientY - rect.top;

		// Stops the drag selecting the header's text instead of moving the window.
		e.preventDefault();
	});

	window.addEventListener('mousemove', function(e)
	{
		if(!chatDragging) return;

		placeChatWindow(e.clientX - chatDragOffsetX, e.clientY - chatDragOffsetY);
	});

	window.addEventListener('mouseup', function() { chatDragging = false; });

	window.addEventListener('resize', function()
	{
		if(chatWindow.hidden) return;

		placeChatWindow(chatWindow.offsetLeft, chatWindow.offsetTop);
	});

	chatToggle.addEventListener('click', function()
	{
		if(!chatWindow.hidden)
		{
			// Only hidden, not reset: a prompt still with the server keeps being polled, so its reply is waiting
			// in the transcript when the window is opened again.
			chatWindow.hidden = true;
			return;
		}

		chatWindow.hidden = false;

		// Opens in the top right corner, clear of anything a page puts in the opposite one. Only on the first
		// open, so it comes back wherever it was last dragged to after that.
		if(!chatWindow.style.left) placeChatWindow(window.innerWidth - chatWindow.offsetWidth - 12, 12);

		refreshChatContexts();
		renderChatLog();

		chatPromptInput.focus();
	});

	chatClose.addEventListener('click', function() { chatWindow.hidden = true; });

	setChatBusy(false);
})();
)CHATSCRIPT";
