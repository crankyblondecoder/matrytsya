#!/usr/bin/env bash
set -euo pipefail

python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install \
    langgraph \
    langgraph-checkpoint \
    langgraph-checkpoint-sqlite \
    langgraph-checkpoint-postgres \
    langchain-ollama

echo "Setup complete. Activate with: source .venv/bin/activate"
