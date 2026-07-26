#!/bin/sh
# ghcr_anonymous_pull.sh <ghcr-ref> <output-file>
#
# Pulls the (single) layer blob of a PUBLIC GHCR OCI artifact using an
# anonymous token — no login required. Used by CI jobs to fetch self-hosted
# BSD dependency snapshots pushed with oras.
#
# Example:
#   sh build/ci/ghcr_anonymous_pull.sh \
#       ghcr.io/pippocao/bqlog-ci-deps/openbsd:7.7-amd64 deps.tar.gz
set -eu

REF="$1"
OUT="$2"

repo_tag="${REF#ghcr.io/}"        # pippocao/bqlog-ci-deps/openbsd:7.7-amd64
repo="${repo_tag%%:*}"            # pippocao/bqlog-ci-deps/openbsd
tag="${repo_tag##*:}"             # 7.7-amd64

token="$(curl -fsSL "https://ghcr.io/token?scope=repository:${repo}:pull" \
    | sed -n 's/.*"token":"\([^"]*\)".*/\1/p')"
if [ -z "$token" ]; then
    echo "error: failed to get anonymous ghcr token for ${repo}" >&2
    exit 1
fi

manifest="$(curl -fsSL \
    -H "Authorization: Bearer ${token}" \
    -H "Accept: application/vnd.oci.image.manifest.v1+json" \
    "https://ghcr.io/v2/${repo}/manifests/${tag}")"

# Our artifacts are single-layer; the layer digest is the last one in the
# manifest (the first digest belongs to the empty config).
digest="$(printf '%s' "$manifest" | tr ',' '\n' \
    | sed -n 's/.*"digest":"\(sha256:[^"]*\)".*/\1/p' | tail -n1)"
if [ -z "$digest" ]; then
    echo "error: no layer digest in manifest for ${REF}" >&2
    exit 1
fi

curl -fsSL -H "Authorization: Bearer ${token}" \
    -o "$OUT" "https://ghcr.io/v2/${repo}/blobs/${digest}"
echo "pulled ${REF} -> ${OUT}"
