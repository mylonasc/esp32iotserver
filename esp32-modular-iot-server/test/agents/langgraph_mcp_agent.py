#!/usr/bin/env python3
import argparse
import asyncio
import os


async def main():
    parser = argparse.ArgumentParser(
        description="LangGraph ReAct agent wired to ESP32 MCP tools"
    )
    parser.add_argument(
        "--mcp-url",
        default=os.environ.get("MCP_URL", "http://192.168.178.30/mcp"),
        help="MCP JSON-RPC endpoint URL",
    )
    parser.add_argument(
        "--provider",
        choices=["auto", "openai", "ollama", "google"],
        default=os.environ.get("LLM_PROVIDER", "auto"),
        help="LLM provider: auto, openai, ollama, or google",
    )
    parser.add_argument(
        "--model",
        default=os.environ.get("LLM_MODEL", "gpt-4o-mini"),
        help="Chat model name (e.g., gpt-4o-mini or gpt-oss-20b)",
    )
    parser.add_argument(
        "--ollama-base-url",
        default=os.environ.get("OLLAMA_BASE_URL", "http://localhost:11434"),
        help="Ollama base URL (e.g., http://localhost:11434)",
    )
    parser.add_argument(
        "--api-key",
        default=os.environ.get("OPENAI_API_KEY") or os.environ.get("GOOGLE_API_KEY"),
        help="OpenAI or Google API key",
    )
    parser.add_argument(
        "--prompt",
        default="Check soil readings, then disable Pump A.",
        help="User prompt to run through the agent",
    )
    args = parser.parse_args()

    # These imports are here to keep startup errors clear.
    from langgraph_mcp_adapters.client import MCPClient

    from agents.esp32_langgraph_agent import make_graph
    from agents.llms.providers import AutoLlmProvider, LlmConfig

    llm = AutoLlmProvider(
        LlmConfig(
            provider=args.provider,
            model=args.model,
            base_url=args.ollama_base_url,
            api_key=args.api_key,
        )
    ).create()

    async with MCPClient(args.mcp_url) as mcp:
        tools = await mcp.list_tools()

        # The agent can now call all MCP tools exposed by the ESP32.
        agent = make_graph(llm, tools)

        result = await agent.ainvoke({"messages": [("user", args.prompt)]})
        print(result["messages"][-1].content)


if __name__ == "__main__":
    asyncio.run(main())
