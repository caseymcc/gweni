#!/bin/bash

forceStart=false
rebuild=false
android=false
while getopts "sra" flag
do
    case "${flag}" in
        s) forceStart=true;;
        r) rebuild=true;;
    esac
done
# Anything after the flags is treated as a command to run inside the container.
shift $((OPTIND-1))

PROJECT_NAME="gweni"
DOCKERFILE="./"

echo "Run ${PROJECT_NAME} build docker"

DOCKER_NAME="${PROJECT_NAME}_build"
CONTAINER_NAME="${PROJECT_NAME}_build_container"
PLATFORM=""

if [ "$rebuild" = true ] || [ "$forceStart" = true ]; then

  running=$(docker container inspect -f '{{.State.Running}}' $CONTAINER_NAME 2>/dev/null)
  if [ "$running" == "true" ]; then
    echo "Stopping container"
    result=$(docker stop $CONTAINER_NAME)
  fi

  exists=$(docker ps -aq -f name=$CONTAINER_NAME)
  if [ "$exists" ]; then
    echo "Removing container"
    result=$(docker rm $CONTAINER_NAME)
  fi

  if [ "$rebuild" = true ]; then
    result=$(docker images -q $DOCKER_NAME )
    if [[ -n "$result" ]]; then
      "Deleting docker"
      result=$(docker rmi $DOCKER_NAME)
    fi
  fi
fi

result=$(docker images -q $DOCKER_NAME )
if [[ ! -n "$result" ]]; then
  echo "Building docker image"
  DOCKER_BUILDKIT=1 docker build \
    --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) \
    -t $DOCKER_NAME ${DOCKERFILE}
fi

exists=$(docker ps -aq -f name=$CONTAINER_NAME)
if [ ! "$exists" ]; then
  echo "Creating docker"

  ARCH=""
  case $(uname -m) in
      i386)   ARCH="386";;
      i686)   ARCH="386";;
      x86_64) ARCH="amd64";;
      arm)    dpkg --print-architecture | grep -q "arm64" && ARCH="arm64" || ARCH="arm";;
      aarch64) ARCH="arm64";;
      armv8b) ARCH="arm64";;
      armv8l) ARCH="arm64";;
  esac
  if [ "$ARCH" == "arm" ] || [ "$ARCH" == "arm64" ]; then
    PLATFORM = "--platform linux/amd64"
  fi

  # vcpkg cache lives in the repo and is mounted as a volume so built packages
  # and downloads persist across container runs.
  mkdir -p "${PWD}/.vcpkg-cache/binary" "${PWD}/.vcpkg-cache/downloads"

  docker create -it --name $CONTAINER_NAME $PLATFORM \
    --net=host \
    -e VCPKG_DEFAULT_BINARY_CACHE=/vcpkg-cache/binary \
    -e VCPKG_DOWNLOADS=/vcpkg-cache/downloads \
    -v ${PWD}:/home/developer/gweni \
    -v ${PWD}/.vcpkg-cache:/vcpkg-cache \
    $DOCKER_NAME
fi

running=$(docker container inspect -f '{{.State.Running}}' $CONTAINER_NAME 2>/dev/null)
if [ "$running" != "true" ]; then
  echo "Starting docker"
  result=$(docker start $CONTAINER_NAME)
fi

if [ "$#" -gt 0 ]; then
  echo "Running in docker: $*"
  docker exec -w /home/developer/gweni $CONTAINER_NAME bash -lc "$*"
else
  echo "Attaching to docker"
  docker exec -it $CONTAINER_NAME bash
fi

