#!/bin/sh
set -eu

fail() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

for cmd in curl jq; do
    require_cmd "$cmd"
done

: "${GITHUB_TOKEN:?GITHUB_TOKEN is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY (owner/repo) is required}"

case "$GITHUB_REPOSITORY" in
    */*) ;;
    *) fail 'GITHUB_REPOSITORY must use owner/repository format' ;;
esac

ASSET=${1:-releases/ReviveEmu-server_v34-prod.tar.gz}
CHECKSUM=${2:-"$ASSET.sha256"}
[ -s "$ASSET" ] || fail "release archive not found: $ASSET"
[ -s "$CHECKSUM" ] || fail "release checksum not found: $CHECKSUM"

TAG=${GITHUB_RELEASE_TAG:-"0.0.${CI_PIPELINE_IID:-0}"}
TITLE=${GITHUB_RELEASE_TITLE:-"ReviveEmu server_v34 ${TAG}"}
API="https://api.github.com/repos/$GITHUB_REPOSITORY"

api_curl() {
    curl --fail-with-body -sS \
        --retry 3 \
        --retry-delay 2 \
        -H "Authorization: Bearer $GITHUB_TOKEN" \
        -H 'Accept: application/vnd.github+json' \
        -H 'X-GitHub-Api-Version: 2022-11-28' \
        "$@"
}

printf '==> Checking GitHub repository access: %s\n' "$GITHUB_REPOSITORY"
repo_json=$(api_curl "$API")
default_branch=$(printf '%s' "$repo_json" | jq -r '.default_branch // empty')
[ -n "$default_branch" ] || fail 'could not determine GitHub default branch'

# Prefer the exact GitLab commit when GitHub already contains it. Otherwise use
# an explicitly configured target, then the GitHub default branch. The release
# notes always record the GitLab SHA that produced the binary.
TARGET=${GITHUB_TARGET_COMMITISH:-}
if [ -n "${CI_COMMIT_SHA:-}" ]; then
    commit_status=$(curl -sS -o /dev/null -w '%{http_code}' \
        --retry 3 \
        --retry-delay 2 \
        -H "Authorization: Bearer $GITHUB_TOKEN" \
        -H 'Accept: application/vnd.github+json' \
        -H 'X-GitHub-Api-Version: 2022-11-28' \
        "$API/commits/$CI_COMMIT_SHA")
    if [ "$commit_status" = 200 ]; then
        TARGET=$CI_COMMIT_SHA
    fi
fi
[ -n "$TARGET" ] || TARGET=$default_branch

NOTES=$(cat <<EOF_NOTES
Production Linux x86/i386 package for CS:S V34 / Build 4100.

GitLab commit: ${CI_COMMIT_SHA:-unknown}
GitLab pipeline: ${CI_PIPELINE_URL:-unknown}
Release target on GitHub: ${TARGET}

Extract the archive directly into the server_v34 root, then run ./cleanup-old-emulators.sh.
EOF_NOTES
)

existing_file=$(mktemp)
trap 'rm -f "$existing_file"' EXIT
status=$(curl -sS -o "$existing_file" -w '%{http_code}' \
    --retry 3 \
    --retry-delay 2 \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H 'Accept: application/vnd.github+json' \
    -H 'X-GitHub-Api-Version: 2022-11-28' \
    "$API/releases/tags/$TAG")

if [ "$status" = 200 ]; then
    RELEASE_ID=$(jq -r '.id' "$existing_file")
    payload=$(jq -n \
        --arg name "$TITLE" \
        --arg body "$NOTES" \
        '{name:$name, body:$body, draft:false, prerelease:false}')
    api_curl -X PATCH "$API/releases/$RELEASE_ID" -d "$payload" >/dev/null
    printf '==> Updating existing GitHub Release %s (id=%s)\n' "$TAG" "$RELEASE_ID"
elif [ "$status" = 404 ]; then
    payload=$(jq -n \
        --arg tag "$TAG" \
        --arg target "$TARGET" \
        --arg name "$TITLE" \
        --arg body "$NOTES" \
        '{tag_name:$tag, target_commitish:$target, name:$name, body:$body, draft:false, prerelease:false}')
    response=$(api_curl -X POST "$API/releases" -d "$payload")
    RELEASE_ID=$(printf '%s' "$response" | jq -r '.id // empty')
    [ -n "$RELEASE_ID" ] || fail 'GitHub did not return a release id'
    printf '==> Created GitHub Release %s (id=%s)\n' "$TAG" "$RELEASE_ID"
else
    cat "$existing_file" >&2
    fail "GitHub release lookup failed with HTTP $status"
fi

upload_asset() {
    path=$1
    content_type=$2
    name=$(basename "$path")

    assets=$(api_curl "$API/releases/$RELEASE_ID/assets?per_page=100")
    old_id=$(printf '%s' "$assets" | jq -r --arg name "$name" '.[] | select(.name == $name) | .id' | head -n 1)
    if [ -n "$old_id" ]; then
        api_curl -X DELETE "$API/releases/assets/$old_id" >/dev/null
    fi

    encoded_name=$(printf '%s' "$name" | jq -sRr @uri)
    response=$(curl --fail-with-body -sS \
        --retry 3 \
        --retry-delay 2 \
        -X POST \
        -H "Authorization: Bearer $GITHUB_TOKEN" \
        -H 'Accept: application/vnd.github+json' \
        -H 'X-GitHub-Api-Version: 2022-11-28' \
        -H "Content-Type: $content_type" \
        --data-binary "@$path" \
        "https://uploads.github.com/repos/$GITHUB_REPOSITORY/releases/$RELEASE_ID/assets?name=$encoded_name")

    uploaded_name=$(printf '%s' "$response" | jq -r '.name // empty')
    uploaded_state=$(printf '%s' "$response" | jq -r '.state // empty')
    [ "$uploaded_name" = "$name" ] || fail "GitHub returned an unexpected uploaded asset name for $name"
    [ "$uploaded_state" = uploaded ] || fail "GitHub asset $name is not in uploaded state (state=$uploaded_state)"
    printf '==> Uploaded GitHub Release asset: %s\n' "$name"
}

upload_asset "$ASSET" 'application/gzip'
upload_asset "$CHECKSUM" 'text/plain'

printf '%s\n' '==> Verifying GitHub Release and assets'
release_json=$(api_curl "$API/releases/tags/$TAG")
verified_id=$(printf '%s' "$release_json" | jq -r '.id // empty')
[ "$verified_id" = "$RELEASE_ID" ] || fail 'GitHub Release verification failed'

assets=$(api_curl "$API/releases/$RELEASE_ID/assets?per_page=100")
for path in "$ASSET" "$CHECKSUM"; do
    name=$(basename "$path")
    count=$(printf '%s' "$assets" | jq -r --arg name "$name" '[.[] | select(.name == $name and .state == "uploaded")] | length')
    [ "$count" -eq 1 ] || fail "GitHub Release asset verification failed for $name (count=$count)"
done

html_url=$(printf '%s' "$release_json" | jq -r '.html_url // empty')
[ -n "$html_url" ] || html_url="https://github.com/$GITHUB_REPOSITORY/releases/tag/$TAG"
printf 'GitHub Release published and verified: %s\n' "$html_url"
