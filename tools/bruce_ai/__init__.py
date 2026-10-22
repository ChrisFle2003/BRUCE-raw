"""Bounded, explicit local assistant for BRUCE; Python standard library only."""

from .config import AssistantConfig, AssistantError, AssistantProfile, load_config

__all__ = ["AssistantConfig", "AssistantError", "AssistantProfile", "load_config"]
