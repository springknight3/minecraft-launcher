#!/usr/bin/env bash
set -euo pipefail

branch="${1:-}"
platform="${2:-windows}"

if [[ -z "$branch" ]]; then
    echo "Usage: $0 <branch> [platform]" >&2
    exit 1
fi

git push origin "$branch"
gh workflow run verify.yml --ref "$branch" -f platform="$platform"
echo "Dispatched verify.yml for branch $branch with platform=$platform"
sleep 10
RUN_ID="$(gh run list --workflow=verify.yml --branch "$branch" --limit 1 --json databaseId --jq '.[0].databaseId')"
echo "Watching run $RUN_ID..."
gh run watch "$RUN_ID" --exit-status
