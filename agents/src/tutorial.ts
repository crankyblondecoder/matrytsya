import { ChatOllama } from "@langchain/ollama";
import { tool } from "@langchain/core/tools";
import { SystemMessage } from "@langchain/core/messages";
import { AIMessage, ToolMessage } from "@langchain/core/messages";
import { HumanMessage } from "@langchain/core/messages";

import {
	StateGraph,
	StateSchema,
	MessagesValue,
	ReducedValue,
	GraphNode,
	ConditionalEdgeRouter,
	START,
	END,
} from "@langchain/langgraph";

import * as z from "zod";

const model = new ChatOllama({

	model: "qwen3-coder:30b",   // or whichever model you have pulled locally
	baseUrl: "http://192.168.20.41:11434",
	temperature: 0,
});

// Define tools
const add = tool(({ a, b }) => a + b, {

  name: "add",

  description: "Add two numbers",

  schema: z.object({

    a: z.number().describe("First number"),
    b: z.number().describe("Second number"),
  }),
});

const multiply = tool(({ a, b }) => a * b, {

  name: "multiply",

  description: "Multiply two numbers",

  schema: z.object({

    a: z.number().describe("First number"),
    b: z.number().describe("Second number"),
  }),
});

const divide = tool(({ a, b }) => a / b, {

  name: "divide",

  description: "Divide two numbers",

  schema: z.object({

    a: z.number().describe("First number"),
    b: z.number().describe("Second number"),
  }),
});

// Augment the LLM with tools
const toolsByName = {

  [add.name]: add,
  [multiply.name]: multiply,
  [divide.name]: divide,
};

const tools = Object.values(toolsByName);

const modelWithTools = model.bindTools(tools);

const MessagesState = new StateSchema({

	messages: MessagesValue,

	llmCalls: new ReducedValue(

		z.number().default(0),
		{ reducer: (x, y) => x + y }
	),
});

const llmCall:GraphNode<typeof MessagesState> = async (state) => {

	const response = await modelWithTools.invoke([

		new SystemMessage( "You are a helpful assistant tasked with performing arithmetic on a set of inputs."

    ), ...state.messages,]);

	return {

		messages: [response],
	    llmCalls: 1,
	};
};

const toolNode:GraphNode<typeof MessagesState> = async (state) => {

	const lastMessage = state.messages.at(-1);

	if(lastMessage == null || !AIMessage.isInstance(lastMessage)) {

		return { messages: [] };
	}

	const result: ToolMessage[] = [];

	for(const toolCall of lastMessage.tool_calls ?? []) {

		const tool = toolsByName[toolCall.name as keyof typeof toolsByName];

		const observation = await tool.invoke(toolCall);

		result.push(observation);
	}

	return { messages: result };
};

const shouldContinue:ConditionalEdgeRouter<typeof MessagesState, Record<string, any>, "toolNode"> = (state) => {

	const lastMessage = state.messages.at(-1);

	// Check if it's an AIMessage before accessing tool_calls.
	if(!lastMessage || !AIMessage.isInstance(lastMessage)) {

		return END;
	}

	// If the LLM makes a tool call, then perform an action.
	if(lastMessage.tool_calls?.length) {

		return "toolNode";
	}

	// Otherwise, we stop (reply to the user).
	return END;
};

const agent = new StateGraph(MessagesState)

	.addNode("llmCall", llmCall)
	.addNode("toolNode", toolNode)
	.addEdge(START, "llmCall")
	.addConditionalEdges("llmCall", shouldContinue, ["toolNode", END])
	.addEdge("toolNode", "llmCall")
	.compile();

const result = await agent.invoke({

	messages: [new HumanMessage("Add 3 and 4.")],
});

for(const message of result.messages) {

	console.log(`[${message.type}]: ${message.text}`);
}

