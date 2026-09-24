#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    qtbase5-dev \
    libopencv-dev \
    ffmpeg \
    v4l-utils

