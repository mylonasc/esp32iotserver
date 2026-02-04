# Agents

This folder holds LangGraph agents and LLM providers that talk to the ESP32 MCP endpoint.

Key pieces:

- `esp32_langgraph_agent/agent.py` exports `make_graph(llm, tools)`.
- `llms/providers.py` defines OpenAI/Ollama/Google/Auto providers.

To extend:

- Add new provider classes in `llms/providers.py`.
- Keep `AutoLlmProvider` priority: Ollama -> OpenAI -> Google.
- Use MCP tools from `/mcp` for device operations.
