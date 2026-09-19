#!/usr/bin/env bash
#
# Applies the local patches this deployment needs against its submodules.
# Run once after `git submodule update --init --recursive`, from anywhere.
#
# Both patches are idempotent: if a patch is already applied it is skipped
# rather than reported as a failure.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

apply_patch() {
    local submodule="$1"
    local patch="$2"
    local target="${REPO_ROOT}/${submodule}"

    if [ ! -d "${target}" ]; then
        echo "error: ${submodule} not found. Run: git submodule update --init --recursive" >&2
        return 1
    fi

    if git -C "${target}" apply --reverse --check "${patch}" 2>/dev/null; then
        echo "skip  ${submodule}: already applied"
    elif git -C "${target}" apply "${patch}"; then
        echo "ok    ${submodule}: applied $(basename "${patch}")"
    else
        echo "error ${submodule}: failed to apply $(basename "${patch}")" >&2
        return 1
    fi
}

apply_patch "lib/fprime" "${REPO_ROOT}/patches/0001-fprime-install-destination.patch"
apply_patch "lib/fprime-zephyr" "${REPO_ROOT}/patches/0002-fprime-zephyr-uart-irq-update.patch"
