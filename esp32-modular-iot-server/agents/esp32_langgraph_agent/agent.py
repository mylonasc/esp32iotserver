from __future__ import annotations

from typing import List
from langgraph.prebuilt import create_react_agent


DEFAULT_ESP32_AGENT_PROMPT="""
You are a device operator for an ESP32 IoT server. 
Use MCP tools to read module state, change configuration, 
and verify results. Be cautious and revert changes when needed.
"""

def make_graph(llm, tools: List, system_prompt = DEFAULT_ESP32_AGENT_PROMPT) -> "CompiledStateGraph":
    """Create a compiled LangGraph ReAct agent for ESP32 MCP tools.

    Args:
        llm: LangChain-compatible chat model.
        tools: List of MCP tools (Tool-like objects) to expose to the agent.

    Returns:
        CompiledStateGraph ready to invoke/ainvoke.
    """
    return create_react_agent(
        llm,
        tools,
        system_prompt=system_prompt,
    )
