from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass
class LlmConfig:
    provider: str = "auto"
    model: str = "gpt-oss-20b"
    base_url: Optional[str] = None
    api_key: Optional[str] = None
    temperature: Optional[float] = None


class BaseLlmProvider:
    def __init__(self, config: LlmConfig) -> None:
        self.config = config

    def create(self):
        raise NotImplementedError


class OllamaLlmProvider(BaseLlmProvider):
    def create(self):
        from langchain_ollama import ChatOllama

        return ChatOllama(
            model=self.config.model or "gpt-oss-20b",
            base_url=self.config.base_url or "http://localhost:11434",
            temperature=self.config.temperature,
        )


class OpenaiLlmProvider(BaseLlmProvider):
    def create(self):
        from langchain_openai import ChatOpenAI

        return ChatOpenAI(
            model=self.config.model or "gpt-4o-mini",
            api_key=self.config.api_key,
            temperature=self.config.temperature,
        )


class GoogleLlmProvider(BaseLlmProvider):
    def create(self):
        from langchain_google_genai import ChatGoogleGenerativeAI

        return ChatGoogleGenerativeAI(
            model=self.config.model or "gemini-1.5-flash",
            api_key=self.config.api_key,
            temperature=self.config.temperature,
        )


class AutoLlmProvider(BaseLlmProvider):
    def create(self):
        preferred = (self.config.provider or "auto").lower()
        if preferred != "auto":
            if preferred == "ollama":
                return OllamaLlmProvider(self.config).create()
            if preferred == "openai":
                return OpenaiLlmProvider(self.config).create()
            if preferred == "google":
                return GoogleLlmProvider(self.config).create()
            raise ValueError(f"Unknown provider: {preferred}")

        # Default priority: Ollama (local) -> OpenAI -> Google
        try:
            return OllamaLlmProvider(self.config).create()
        except Exception:
            pass

        try:
            return OpenaiLlmProvider(self.config).create()
        except Exception:
            pass

        return GoogleLlmProvider(self.config).create()
