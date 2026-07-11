#!/bin/bash

set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 "${SCRIPT_DIR}/run.py" \
  --config "${SCRIPT_DIR}/targets.example.json" \
  --target local-podman \
  --build-image \
  --interactive \
  --container-workdir /gaia-ecs/vm \
  -- bash