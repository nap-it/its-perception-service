#!/bin/bash
# build_docker_images.sh
# This script builds several docker images with buildx and logs each step.
# Usage: ./build_docker_images.sh [push]
# If "push" is provided as an argument, the script will use --push.
# Otherwise, it will use --load.

# Log file path
LOGFILE="build_docker_images.log"

# Check for build mode argument
if [ "$1" == "push" ]; then
    BUILD_FLAG="--push"
    echo "Building with push mode..."
else
    BUILD_FLAG="--load"
    echo "Building with load mode..."
fi

# Function to print a timestamped message to stdout and the log file.
log() {
  echo "$(date +"%Y-%m-%d %H:%M:%S") : $*" | tee -a "$LOGFILE"
}

# Function to build a docker image.
# Arguments:
#   $1 -> Directory in which to run the build.
#   $2 -> Docker tag to use.
build_image() {
  local build_dir="$1"
  local tag="$2"

  log "---------------------------------------------------"
  log "Starting build for image: $tag"
  log "Directory: $build_dir"
  log "---------------------------------------------------"

  if [ ! -d "$build_dir" ]; then
    log "ERROR: Directory $build_dir does not exist."
    return 1
  fi

  pushd "$build_dir" > /dev/null || { log "ERROR: Could not change directory to $build_dir"; return 1; }

  local start_time=$(date +%s)
  log "Running docker buildx build command..."
  
  docker buildx build --platform=linux/arm64,linux/amd64 $BUILD_FLAG --tag "$tag" .
  local exit_code=$?

  local end_time=$(date +%s)
  local duration=$((end_time - start_time))

  if [ $exit_code -ne 0 ]; then
    log "ERROR: Build failed for image: $tag. Duration: ${duration} seconds."
  else
    log "SUCCESS: Build completed for image: $tag. Duration: ${duration} seconds."
  fi

  popd > /dev/null || log "WARNING: Could not return to previous directory."
  return $exit_code
}

# Start builds sequentially.
build_image "./generation" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/generation:dcc" || exit 1
#build_image "./sensor-adapters/camera-adapter" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/camera-adapter:dcc" || exit 1
#build_image "./sensor-adapters/radar-adapter" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/radar-adapter:dcc" || exit 1
#build_image "./sensor-adapters/bike-adapter" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/bike-adapter:dcc" || exit 1
#build_image "./sensor-adapters/autoware-adapter" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/autoware-adapter:dcc" || exit 1
#build_image "./processing" "code.nap.av.it.pt:5050/mobility-networks/cps-v2/processing:dcc" || exit 1

log "All builds completed."
