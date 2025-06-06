#!/bin/bash
# build_docker_images.sh
# This script builds several docker images with buildx and logs each step.
# Usage: ./build_docker_images.sh [push]
# If "push" is provided as an argument, the script will use --push (multi-arch).
# Otherwise, it will use --load (single-arch amd64).

LOGFILE="build_docker_images.log"
: > "$LOGFILE"
ORIG_DIR=$(pwd)

if [ "$1" == "push" ]; then
    BUILD_FLAG="--push"
    echo "Building with push mode (multi-arch)…"
else
    BUILD_FLAG="--load"
    echo "Building with load mode (single-arch amd64)…"
fi

log() {
  pushd "$ORIG_DIR" >/dev/null
  echo "$(date +"%Y-%m-%d %H:%M:%S") : $*" | tee -a "$LOGFILE"
  popd >/dev/null
}

build_image() {
  local build_dir="$1"
  local tag="$2"

  log "---------------------------------------------------"
  log "Starting build for image: $tag"
  log "Directory: $build_dir"

  if [ ! -d "$build_dir" ]; then
    log "ERROR: Directory $build_dir does not exist."
    return 1
  fi
  
  pushd "$build_dir" >/dev/null || { log "ERROR: Could not cd to $build_dir"; return 1; }
  log "Running docker buildx build command..."
  local start_time=$(date +%s)
  
  if [ "$BUILD_FLAG" == "--load" ]; then
    # Only build amd64 slice when loading into local Docker
    docker buildx build \
      --platform=linux/amd64 \
      $BUILD_FLAG \
      --tag "$tag" \
      .
  else
    # Build and push both arm64 and amd64
    docker buildx build \
      --platform=linux/arm64,linux/amd64 \
      $BUILD_FLAG \
      --tag "$tag" \
      .
  fi

  local exit_code=$?
  local end_time=$(date +%s)
  local duration=$((end_time - start_time))

  if [ $exit_code -ne 0 ]; then
    log "ERROR: Build failed for image: $tag. Duration: ${duration} seconds."
  else
    log "SUCCESS: Build completed for image: $tag. Duration: ${duration} seconds."
  fi

  popd >/dev/null || log "WARNING: Could not return to previous directory."
  log "---------------------------------------------------"
  return $exit_code
}

# Now run each build. If any fails, exit immediately.
build_image "./generation"           "code.nap.av.it.pt:5050/mobility-networks/cps-v2/generation:latest" || exit 1
build_image "./sensor-adapters/camera-adapter"  "code.nap.av.it.pt:5050/mobility-networks/cps-v2/camera-adapter:latest" || exit 1
build_image "./sensor-adapters/radar-adapter"   "code.nap.av.it.pt:5050/mobility-networks/cps-v2/radar-adapter:latest" || exit 1
build_image "./sensor-adapters/bike-adapter"    "code.nap.av.it.pt:5050/mobility-networks/cps-v2/bike-adapter:latest" || exit 1
build_image "./sensor-adapters/autoware-adapter" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/autoware-adapter:latest" || exit 1
build_image "./processing"         "code.nap.av.it.pt:5050/mobility-networks/cps-v2/processing:latest" || exit 1

log "All builds completed."
