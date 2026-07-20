# ESP32 MCP Agents

This folder contains LangGraph-based agent helpers and LLM provider wrappers for the ESP32 MCP endpoint exposed at `POST /mcp`.

## Structure

- `esp32_langgraph_agent/`: exports `make_graph(llm, tools)` for creating a ReAct agent.
- `llms/`: provider helpers for Ollama, OpenAI, Google, and auto-selection.
- `../../test/agents/`: live-device scripts that exercise MCP tools and run the sample agent.

## Setup

From `esp32-modular-iot-server/`, install the local apps package plus agent dependencies:

```bash
pip install -e 'apps[agents]'
```

If you do not install the package, run scripts with `PYTHONPATH=apps` so Python can import the local `agents` package.

## Run The Sample Agent

From `esp32-modular-iot-server/`:

```bash
PYTHONPATH=apps python3 test/agents/langgraph_mcp_agent.py --mcp-url http://192.168.178.30/mcp
```

The default prompt is intentionally operational and may change device config. Override it for safer reads:

```bash
PYTHONPATH=apps python3 test/agents/langgraph_mcp_agent.py \
  --mcp-url http://esp32.local/mcp \
  --prompt "List modules and report current status only."
```

## Providers

`AutoLlmProvider` chooses in this order:

1. Ollama
2. OpenAI
3. Google

You can force a provider with `LLM_PROVIDER` or `--provider`.

### OpenAI

```bash
export OPENAI_API_KEY=...
PYTHONPATH=apps python3 test/agents/langgraph_mcp_agent.py --provider openai --model gpt-4o-mini
```

### Ollama

```bash
PYTHONPATH=apps python3 test/agents/langgraph_mcp_agent.py \
  --provider ollama \
  --model gpt-oss-20b \
  --ollama-base-url http://localhost:11434
```

### Google

```bash
export GOOGLE_API_KEY=...
PYTHONPATH=apps python3 test/agents/langgraph_mcp_agent.py --provider google --model gemini-1.5-flash
```

## MCP Smoke Test

The smoke test uses the standard library only and verifies core MCP tools plus pump/soil config round trips:

```bash
python3 test/agents/mcp_tools_test.py --base-url http://esp32.local
```

Use `--check-ui` to also fetch `/config` and verify updated values appear in the page.
