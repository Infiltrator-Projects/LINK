#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Delete one explicitly audited branch only while its SHA and patch integration
# still match the caller's expectations. Never select branches by wildcard.
set -euo pipefail
branch=${1:?branch name required}
expected=${2:?audited commit required}
[[ "$branch" != main && "$branch" != master ]]
git check-ref-format "refs/heads/$branch"
[[ "$expected" =~ ^[0-9a-f]{40}$ ]]
if remote=$(git ls-remote --exit-code --heads origin "refs/heads/$branch"); then
    [[ "${remote%%[[:space:]]*}" == "$expected" ]] || {
        echo 'Audited branch has changed; refusing deletion.' >&2
        exit 1
    }
else
    status=$?
    if [[ "$status" == 2 ]]; then
        echo 'Audited branch is already absent.'
        exit 0
    fi
    exit "$status"
fi
git fetch --no-tags origin "+refs/heads/main:refs/remotes/origin/main" "+refs/heads/$branch:refs/remotes/origin/$branch"
[[ "$(git rev-parse "refs/remotes/origin/$branch")" == "$expected" ]]
# git cherry detects patch-equivalent integration, including squash/cherry-pick
# histories. A plus sign is unmerged work and prevents deletion.
patches=$(git cherry refs/remotes/origin/main "$expected")
if grep -q '^+' <<<"$patches"; then
    echo 'Audited branch contains work absent from main; refusing deletion.' >&2
    exit 1
fi
if ! git merge-base --is-ancestor "$expected" refs/remotes/origin/main &&
   [[ -n "$(git rev-list --merges "refs/remotes/origin/main..$expected")" ]]; then
    echo 'Unmerged merge commits require a separate audit; refusing deletion.' >&2
    exit 1
fi
# The lease also protects changes made after the checks above.
git push --force-with-lease="refs/heads/$branch:$expected" origin ":refs/heads/$branch"
