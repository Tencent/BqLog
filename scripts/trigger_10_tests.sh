#!/bin/bash

# Script to help trigger GitHub Actions manually
# Note: This requires the GitHub CLI (gh) to be installed and authenticated

REPO="Tencent/BqLog"
WORKFLOW="test.yml"
BRANCH="feature/performance_mode"
RUNS=10

echo "🚀 Starting 10 Auto Test runs on $BRANCH branch"
echo "Repository: $REPO"
echo "Workflow: $WORKFLOW"
echo ""

# Check if gh CLI is available
if ! command -v gh &> /dev/null; then
    echo "❌ GitHub CLI (gh) is not installed."
    echo "Please install it from: https://cli.github.com/"
    echo ""
    echo "Alternative: Manually trigger the workflow via GitHub web interface:"
    echo "1. Go to https://github.com/$REPO/actions"
    echo "2. Select the 'AutoTest' workflow"
    echo "3. Click 'Run workflow'"
    echo "4. Set branch to '$BRANCH'"
    echo "5. Repeat 10 times"
    exit 1
fi

# Check authentication
if ! gh auth status &> /dev/null; then
    echo "❌ GitHub CLI is not authenticated."
    echo "Please run: gh auth login"
    exit 1
fi

echo "✅ GitHub CLI is ready"
echo ""

# Function to trigger a workflow run
trigger_run() {
    local run_num=$1
    echo "🔥 Triggering run #$run_num..."
    
    if gh workflow run "$WORKFLOW" --repo "$REPO" --ref "$BRANCH"; then
        echo "✅ Run #$run_num triggered successfully"
    else
        echo "❌ Failed to trigger run #$run_num"
        return 1
    fi
}

# Trigger all runs
for i in $(seq 1 $RUNS); do
    trigger_run $i
    
    # Add a small delay to avoid rate limiting
    sleep 2
    echo ""
done

echo "🎉 All $RUNS workflow runs have been triggered!"
echo ""
echo "📊 You can monitor the progress at:"
echo "https://github.com/$REPO/actions"
echo ""
echo "💡 Tip: Use the new 'Run Auto Test 10 Times on Performance Mode' workflow"
echo "for automated parallel execution with built-in result summary."