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

RUN_NUMBER=${GITHUB_RUN_NUMBER:-0}
TAG=${GITHUB_RELEASE_TAG:-"0.0.${RUN_NUMBER}"}
TITLE=${GITHUB_RELEASE_TITLE:-"ReviveEmu server_v34 ${TAG}"}
SOURCE_COMMIT=${GITHUB_SHA:-}
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

TARGET=${GITHUB_TARGET_COMMITISH:-${SOURCE_COMMIT:-$default_branch}}
commit_json=$(api_curl "$API/commits/$TARGET")
TARGET_SHA=$(printf '%s' "$commit_json" | jq -r '.sha // empty')
[ -n "$TARGET_SHA" ] || fail "could not resolve release target: $TARGET"

NOTES=$(cat <<EOF_NOTES
Production Linux x86/i386 package for CS:S V34 / Build 4100.

Build commit: ${SOURCE_COMMIT:-$TARGET_SHA}
Extract the archive directly into the server_v34 root, then run ./cleanup-old-emulators.sh.
EOF_NOTES
)

# Create the tag explicitly before the Release. This keeps the Release bound to
# the exact workflow commit and avoids GitHub trying to synthesize a tag from a
# branch during release creation.
tag_file=$(mktemp)
release_file=$(mktemp)
trap 'rm -f "$tag_file" "$release_file"' EXIT

tag_status=$(curl -sS -o "$tag_file" -w '%{http_code}' \
    --retry 3 \
    --retry-delay 2 \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H 'Accept: application/vnd.github+json' \
    -H 'X-GitHub-Api-Version: 2022-11-28' \
    "$API/git/ref/tags/$TAG")

if [ "$tag_status" = 404 ]; then
    tag_payload=$(jq -n \
        --arg ref "refs/tags/$TAG" \
        --arg sha "$TARGET_SHA" \
        '{ref:$ref, sha:$sha}')
    api_curl -X POST "$API/git/refs" -d "$tag_payload" >/dev/null
    printf '==> Created GitHub tag %s at %s\n' "$TAG" "$TARGET_SHA"
elif [ "$tag_status" = 200 ]; then
    existing_tag_sha=$(jq -r '.object.sha // empty' "$tag_file")
    [ -n "$existing_tag_sha" ] || fail "could not resolve existing tag: $TAG"
    [ "$existing_tag_sha" = "$TARGET_SHA" ] || \
        fail "tag $TAG already points to $existing_tag_sha instead of $TARGET_SHA"
    printf '==> Reusing existing GitHub tag %s\n' "$TAG"
else
    cat "$tag_file" >&2
    fail "GitHub tag lookup failed with HTTP $tag_status"
fi

status=$(curl -sS -o "$release_file" -w '%{http_code}' \
    --retry 3 \
    --retry-delay 2 \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H 'Accept: application/vnd.github+json' \
    -H 'X-GitHub-Api-Version: 2022-11-28' \
    "$API/releases/tags/$TAG")

if [ "$status" = 200 ]; then
    RELEASE_ID=$(jq -r '.id // empty' "$release_file")
    [ -n "$RELEASE_ID" ] || fail 'existing GitHub Release has no id'
    payload=$(jq -n \
        --arg name "$TITLE" \
        --arg body "$NOTES" \
        '{name:$name, body:$body, draft:false, prerelease:false}')
    api_curl -X PATCH "$API/releases/$RELEASE_ID" -d "$payload" >/dev/null
    printf '==> Updating existing GitHub Release %s (id=%s)\n' "$TAG" "$RELEASE_ID"
elif [ "$status" = 404 ]; then
    payload=$(jq -n \
        --arg tag "$TAG" \
        --arg name "$TITLE" \
        --arg body "$NOTES" \
        '{tag_name:$tag, name:$name, body:$body, draft:false, prerelease:false}')
    response=$(api_curl -X POST "$API/releases" -d "$payload")
    RELEASE_ID=$(printf '%s' "$response" | jq -r '.id // empty')
    [ -n "$RELEASE_ID" ] || fail 'GitHub did not return a release id'
    printf '==> Created GitHub Release %s (id=%s)\n' "$TAG" "$RELEASE_ID"
else
    cat "$release_file" >&2
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
