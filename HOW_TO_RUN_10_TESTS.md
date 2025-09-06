# How to Run Auto Test 10 Times on feature/performance_mode Branch

This document explains how to use the new workflow to run the Auto Test workflow 10 times on the `feature/performance_mode` branch.

## Method 1: Using the New Workflow (Recommended)

### Step 1: Switch to the feature/performance_mode branch

First, make sure the workflow file is available on the target branch:

1. Go to the GitHub repository: https://github.com/Tencent/BqLog
2. Navigate to the `feature/performance_mode` branch
3. Merge or cherry-pick the new workflow file to this branch

### Step 2: Trigger the Workflow

1. Go to the **Actions** tab in the GitHub repository
2. Find the workflow "Run Auto Test 10 Times on Performance Mode" in the left sidebar
3. Click on the workflow name
4. Click the **Run workflow** button
5. Make sure the target branch is set to `feature/performance_mode`
6. Click **Run workflow** to start the execution

### Features

- **Parallel Execution**: All 10 test runs execute simultaneously for faster completion
- **Comprehensive Summary**: Shows the results of all 10 runs and calculates success rate
- **Detailed Logging**: Each test run is clearly labeled and tracked
- **Failure Resilience**: Even if some tests fail, the workflow continues and provides a summary

## Method 2: Manual Triggering (Alternative)

If you prefer to run the tests manually:

1. Go to the **Actions** tab
2. Find the **AutoTest** workflow
3. Click **Run workflow** 
4. Set the branch to `feature/performance_mode`
5. Repeat this process 10 times

Note: This method requires manual tracking of results and is more time-consuming.

## Expected Output

The workflow will:
1. Execute the complete Auto Test suite 10 times in parallel
2. Test all platforms (Windows x86_64, Windows ARM64, Linux Ubuntu x86_64, Linux Ubuntu ARM64, Linux Debian, macOS, FreeBSD)
3. Test all compiler combinations (MSVC, Clang, GCC)
4. Test all C++ standards (11, 14, 17, 20, 23)
5. Provide a comprehensive summary with success rate

## Interpreting Results

The summary job will show:
- Individual result for each of the 10 runs (success/failure)
- Total number of successful runs
- Success rate percentage
- Detailed failure information for debugging

## Troubleshooting

If the workflow fails to appear:
1. Ensure the workflow file is present in `.github/workflows/` directory on the target branch
2. Check that the YAML syntax is valid
3. Verify you have necessary permissions to run workflows

## Files Added

- `.github/workflows/run_auto_test_10_times.yml` - The main workflow file
- `HOW_TO_RUN_10_TESTS.md` - This documentation file