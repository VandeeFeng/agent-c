# Agent-C

A ultra-lightweight AI agent written in C that communicates with OpenRouter API and executes shell commands.

## Features

- **Tool Calling**: Execute shell commands directly through AI responses
- **Skill System**: Discover and execute predefined skill scripts from `~/.agent-c/skills/` directory
- **Conversation Memory**: Sliding window memory management for efficient operation
- **Cross-Platform**: macOS and Linux

## Quick Start

### Prerequisites

- GCC compiler
- cJSON library
- curl command-line tool
- OpenRouter or OpenAI API key
- macOS: gzexe (usually pre-installed)
- Linux: upx (optional, for compression)

### Build

```bash
make
```

### Skill System
[Introducing Agent Skills | Claude](https://claude.com/blog/skills)

[Equipping agents for the real world with Agent Skills \ Anthropic](https://www.anthropic.com/engineering/equipping-agents-for-the-real-world-with-agent-skills)

Inspired by Claude's skill system, this project implements similar functionality to execute custom scripts

Create skills in `~/.agent-c/skills/` with the following structure:

```
~/.agent-c/skills/
└── git/
    ├── SKILL.md
    └── scripts/
        └── commit_analyzer.sh
```

Each `SKILL.md` file should contain:
- Skill description and usage examples
- Available scripts and their arguments
- Best practices and notes

The agent will automatically discover skills and make them available during conversation.

### Setup

Set your API key:

```bash
export AGENTC_API_KEY=your_api_key_here
```

**Optional**: Set a custom API endpoint (defaults to OpenRouter):

```bash
export AGENTC_BASE_URL=https://your-custom-api.com/v1/chat/completions
```

**Optional**: Set a custom model (defaults to qwen/qwen3-coder):

```bash
export AGENTC_MODEL=your_custom_model
```

**Optional**: Configure specific providers (defaults to cerebras):

```bash
# Use specific providers (comma-separated)
export AGENTC_OP_PROVIDER="cerebras,openai"

# Disable provider filtering (use all available providers)
export AGENTC_OP_PROVIDER=false
```

### Run

```bash
./agent-c
```

## License

**CC0 - "No Rights Reserved"**
