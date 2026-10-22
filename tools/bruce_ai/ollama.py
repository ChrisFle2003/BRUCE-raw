"""Bounded literal-loopback Ollama HTTP transport; no subprocesses or downloads."""

import http.client
import json
import math
import socket
import threading
import time

from .config import AssistantError, AssistantProfile, strict_json_loads, validate_ollama_url


MAX_REQUEST_BYTES = 256 * 1024
MAX_RESPONSE_BYTES = 1024 * 1024
MAX_CONNECT_TIMEOUT_MS = 10000
MAX_REQUEST_TIMEOUT_MS = 120000


class LocalOllamaClient:
    """Verify /api/show local GGUF metadata before every embed/chat request."""

    def _require_local_model(self, model: str, profile: AssistantProfile):
        # /api/show documents local GGUF metadata. Current ShowResponse also
        # identifies cloud aliases with remote_host/remote_model (omitempty).
        # https://github.com/ollama/ollama/blob/main/docs/api.md#show-model-information
        # https://github.com/ollama/ollama/blob/main/api/types.go (ShowResponse)
        # No inference content is sent in this preflight, and no result is cached.
        metadata = self._post("/api/show", {"model": model}, profile)
        details = metadata.get("details")
        info = metadata.get("model_info")
        if (any(metadata.get(field, "") != "" for field in ("remote_host", "remote_model"))
                or not isinstance(details, dict) or details.get("format") != "gguf"
                or not isinstance(info, dict)
                or not isinstance(info.get("general.architecture"), str)
                or not info["general.architecture"].strip()):
            raise AssistantError("OllamaModelNotLocal")

    def _post(self, endpoint: str, payload: dict, profile: AssistantProfile) -> dict:
        host, port = validate_ollama_url(profile.ollama_url)
        try:
            body = json.dumps(payload, ensure_ascii=False, allow_nan=False).encode("utf-8")
        except (TypeError, ValueError, UnicodeError, RecursionError):
            raise AssistantError("OllamaInvalidRequest") from None
        if len(body) > MAX_REQUEST_BYTES:
            raise AssistantError("OllamaInvalidRequest")
        if endpoint in ("/api/embed", "/api/chat"):
            self._require_local_model(payload["model"], profile)

        request_seconds = min(profile.request_timeout_ms, MAX_REQUEST_TIMEOUT_MS) / 1000
        connect_seconds = min(profile.connect_timeout_ms, MAX_CONNECT_TIMEOUT_MS) / 1000
        started = time.monotonic()
        connection = http.client.HTTPConnection(host, port,
                                               timeout=min(connect_seconds, request_seconds))
        expired = threading.Event()
        timer = None
        try:
            # Direct literal address: environment proxy settings and redirect handlers
            # cannot alter the destination. HTTPConnection never follows redirects.
            connection.connect()
            connected_socket = connection.sock
            remaining = request_seconds - (time.monotonic() - started)
            if remaining <= 0:
                raise AssistantError("OllamaTimeout")
            connected_socket.settimeout(remaining)

            def expire():
                expired.set()
                try:
                    connected_socket.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

            # A socket timeout alone restarts for every arriving byte. Shutdown at
            # the absolute deadline also bounds slow headers and trickled bodies.
            timer = threading.Timer(remaining, expire)
            timer.daemon = True
            timer.start()
            connection.request("POST", endpoint, body=body,
                               headers={"Content-Type": "application/json", "Accept": "application/json"})
            with connection.getresponse() as response:
                if 300 <= response.status < 400:
                    raise AssistantError("OllamaInvalidResponse")
                if response.status == 404:
                    raise AssistantError("OllamaModelUnavailable")
                if response.status != 200:
                    raise AssistantError("OllamaUnavailable")
                length = response.getheader("Content-Length")
                if length is not None and (not length.isdecimal() or int(length) > MAX_RESPONSE_BYTES):
                    raise AssistantError("OllamaInvalidResponse")
                raw = response.read(MAX_RESPONSE_BYTES + 1)
                if expired.is_set() or time.monotonic() - started >= request_seconds:
                    raise AssistantError("OllamaTimeout")
                if (len(raw) > MAX_RESPONSE_BYTES
                        or (length is not None and len(raw) != int(length))):
                    raise AssistantError("OllamaInvalidResponse")
            document = strict_json_loads(raw.decode("utf-8"))
            if not isinstance(document, dict):
                raise AssistantError("OllamaInvalidResponse")
            return document
        except AssistantError:
            raise
        except TimeoutError:
            raise AssistantError("OllamaTimeout") from None
        except http.client.HTTPException:
            code = "OllamaTimeout" if expired.is_set() else "OllamaInvalidResponse"
            raise AssistantError(code) from None
        except OSError:
            code = "OllamaTimeout" if expired.is_set() else "OllamaUnavailable"
            raise AssistantError(code) from None
        except (ValueError, UnicodeError, RecursionError):
            raise AssistantError("OllamaInvalidResponse") from None
        finally:
            if timer is not None:
                timer.cancel()
            connection.close()

    def embed(self, text: str, profile: AssistantProfile) -> list[float]:
        if not isinstance(text, str) or not text:
            raise AssistantError("OllamaInvalidRequest")
        document = self._post("/api/embed", {"model": profile.embedding_model, "input": text}, profile)
        embeddings = document.get("embeddings")
        if (not isinstance(embeddings, list) or len(embeddings) != 1
                or not isinstance(embeddings[0], list) or not embeddings[0]):
            raise AssistantError("OllamaInvalidResponse")
        vector = embeddings[0]
        try:
            if any(type(value) not in (int, float) or not math.isfinite(value) for value in vector):
                raise AssistantError("OllamaInvalidResponse")
            return [float(value) for value in vector]
        except (OverflowError, ValueError):
            raise AssistantError("OllamaInvalidResponse") from None

    def chat_json(self, messages: list[dict[str, str]], profile: AssistantProfile,
                  response_schema: dict | None = None) -> dict:
        if not isinstance(messages, list) or not messages:
            raise AssistantError("OllamaInvalidRequest")
        for message in messages:
            if (not isinstance(message, dict) or set(message) != {"role", "content"}
                    or message["role"] not in ("system", "user", "assistant")
                    or not isinstance(message["content"], str)):
                raise AssistantError("OllamaInvalidRequest")
        if response_schema is not None and type(response_schema) is not dict:
            raise AssistantError("OllamaInvalidRequest")
        document = self._post("/api/chat", {"model": profile.response_model,
                              "messages": messages, "stream": False,
                              "format": response_schema if response_schema is not None else "json",
                              "think": False}, profile)
        message = document.get("message")
        if (document.get("done") is not True or not isinstance(message, dict)
                or message.get("role") != "assistant" or message.get("refusal")
                or message.get("tool_calls") or not isinstance(message.get("content"), str)
                or document.get("done_reason", "stop") != "stop"):
            raise AssistantError("OllamaInvalidResponse")
        try:
            result = strict_json_loads(message["content"])
        except (ValueError, RecursionError):
            raise AssistantError("OllamaInvalidResponse") from None
        if not isinstance(result, dict):
            raise AssistantError("OllamaInvalidResponse")
        return result
