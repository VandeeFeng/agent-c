#!/bin/bash

# Git Commit Analyzer
# Analyzes recent commits and provides statistics

DAYS=7
AUTHOR=""
REPO_PATH="."

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --days=*)
            DAYS="${1#*=}"
            shift
            ;;
        --days)
            DAYS="$2"
            shift 2
            ;;
        --author=*)
            AUTHOR="${1#*=}"
            shift
            ;;
        --author)
            AUTHOR="$2"
            shift 2
            ;;
        --path=*)
            REPO_PATH="${1#*=}"
            shift
            ;;
        --path)
            REPO_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if we're in a git repository
if ! git -C "$REPO_PATH" rev-parse --git-dir > /dev/null 2>&1; then
    echo "Error: Not a git repository: $REPO_PATH"
    exit 1
fi

echo "📊 Git Commit Analysis"
echo "======================"
echo "Repository: $(git -C "$REPO_PATH" remote get-url origin 2>/dev/null || echo "Local repository")"
echo "Analysis period: Last $DAYS days"
if [ -n "$AUTHOR" ]; then
    echo "Filtering by author: $AUTHOR"
fi
echo

# Build git log command
LOG_CMD="git -C '$REPO_PATH' log --since='$DAYS days ago' --pretty=format:'%h|%an|%ad|%s' --date=short"
if [ -n "$AUTHOR" ]; then
    LOG_CMD="$LOG_CMD --author='$AUTHOR'"
fi

# Get commit data
commit_data=$(eval "$LOG_CMD")
commit_count=$(echo "$commit_data" | wc -l)

if [ "$commit_count" -eq 0 ]; then
    echo "No commits found in the specified period."
    exit 0
fi

echo "Total commits: $commit_count"
echo

# Author statistics
echo "👥 Author Statistics:"
echo "$commit_data" | cut -d'|' -f2 | sort | uniq -c | sort -nr | while read count author; do
    percentage=$((count * 100 / commit_count))
    printf "  %-20s: %3d commits (%3d%%)\n" "$author" "$count" "$percentage"
done
echo

# Daily activity
echo "📅 Daily Activity:"
echo "$commit_data" | cut -d'|' -f3 | sort | uniq -c | sort -n | while read count date; do
    printf "  %-12s: %3d commits\n" "$date" "$count"
done
echo

# Recent commits
echo "📝 Recent Commits:"
echo "$commit_data" | head -10 | while IFS='|' read hash author date subject; do
    printf "  %s (%s) %s: %s\n" "$hash" "$date" "$author" "$subject"
done

if [ "$commit_count" -gt 10 ]; then
    echo "  ... and $((commit_count - 10)) more commits"
fi

echo
echo "✅ Analysis complete"