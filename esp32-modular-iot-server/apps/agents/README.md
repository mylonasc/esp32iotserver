# Agents

This folder contains LangGraph-based agents and LLM provider helpers for the ESP32 MCP server.

## Structure

- `esp32_langgraph_agent/` – ESP32 ReAct agent with `make_graph(llm, tools)`
- `llms/` – LLM provider helpers (Ollama, OpenAI, Google, Auto)

## Quick Start

Install dependencies:

```bash
pip install langgraph langgraph-mcp-adapters langchain-openai langchain-ollama langchain-google-genai
```

Run the sample script:

```bash
python3 test/langgraph_mcp_agent.py --mcp-url http://192.168.178.30/mcp
```

## Providers

The `AutoLlmProvider` chooses in this order:

1) Ollama (default model: `gpt-oss-20b`)
2) OpenAI
3) Google

You can force a provider via `LLM_PROVIDER` or `--provider`.

### OpenAI

```bash
export OPENAI_API_KEY=...
python3 test/langgraph_mcp_agent.py --provider openai --model gpt-4o-mini
```

### Ollama

```bash
python3 test/langgraph_mcp_agent.py --provider ollama --model gpt-oss-20b --ollama-base-url http://localhost:11434
```

### Google

```bash
export GOOGLE_API_KEY=...
python3 test/langgraph_mcp_agent.py --provider google --model gemini-1.5-flash
```
