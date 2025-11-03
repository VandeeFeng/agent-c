---
name: git
description: Git repository analysis and commit history tools
keywords: git, repository, commit, analysis, history, blame
---

# Git Skill

A Git analysis toolkit for inspecting repository history, commits, and code changes.

## Capabilities

- Analyze commit history and patterns
- Get repository statistics
- Perform code blame analysis
- Find commit authors and contribution data
- Analyze branch structure and merges

## Usage Examples

### Analyze recent commits
```
execute_skill: git commit_analyzer --days=7
```

### Get repository statistics
```
execute_skill: git repo_stats
```

### Find who modified a file
```
execute_skill: git file_blame src/main.c
```

### Analyze commit patterns
```
execute_skill: git commit_patterns --author=john
```

## Scripts

- `commit_analyzer.sh`: Analyze commit history and patterns
- `repo_stats.sh`: Get repository statistics
- `file_blame.sh`: Analyze file modifications
- `commit_patterns.sh`: Find commit patterns and trends

## Requirements

- Requires git to be installed and available in PATH
- Works with any git repository

## Notes

This skill provides insights into repository activity and development patterns. Useful for project management and code review processes.