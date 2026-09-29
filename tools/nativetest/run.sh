#!/bin/sh
# Builds and runs every *_test.cpp here inside the acore build image. No server or build tree needed.
#
# usage: tools/nativetest/run.sh
#
# env: PB_IMAGE, PB_SANITIZE (default thread; address,undefined also works)

set -e
export MSYS_NO_PATHCONV=1  # or Git Bash rewrites the container-side mount paths

MODULE=$(cd "$(dirname "$0")/../.." && { pwd -W 2>/dev/null || pwd; })
IMAGE=${PB_IMAGE:-acore/ac-wotlk-build:master}
SANITIZE=${PB_SANITIZE:-thread}

docker run --rm -e SANITIZE="$SANITIZE" -v "$MODULE:/module:ro" --entrypoint sh "$IMAGE" -c '
set -e
for test in /module/tools/nativetest/*_test.cpp; do
    name=$(basename "$test" .cpp)
    c++ -std=gnu++20 -Wall -Wextra -Werror -g -O1 -pthread -fsanitize="$SANITIZE" \
        -I /module/src/Ai/Raid -I /module/src/Mgr/Item -I /module/src/Bot/Factory "$test" -o "/tmp/$name"
    "/tmp/$name"
done
'
