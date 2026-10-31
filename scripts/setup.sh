#!/bin/bash
# Installs ROS 2 Jazzy, MAVROS and build tools on Ubuntu 24.04 (Jetson with JetPack 7 or dev machine).
# Usage: scripts/setup.sh            -> ros-base (Jetson)
#        scripts/setup.sh --desktop  -> desktop with RViz and GUI tools (dev machine)
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

distro=jazzy
ros_pkg=ros-$distro-ros-base
if [[ "${1:-}" == "--desktop" ]]; then
    ros_pkg=ros-$distro-desktop
fi

codename=$(. /etc/os-release && echo "$UBUNTU_CODENAME")
if [[ "$codename" != "noble" ]]; then
    echo "ROS 2 Jazzy needs Ubuntu 24.04 (noble), found: $codename" >&2
    exit 1
fi

sudo -E apt-get update
sudo -E apt-get install -y software-properties-common curl locales
sudo locale-gen en_US en_US.UTF-8
sudo add-apt-repository -y universe

tmp=$(mktemp -d)
ver=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest \
    | grep -F '"tag_name"' | awk -F'"' '{print $4}')
curl -fL -o "$tmp/ros2-apt-source.deb" \
    "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ver}/ros2-apt-source_${ver}.${codename}_all.deb"
sudo dpkg -i "$tmp/ros2-apt-source.deb"
rm -rf "$tmp"

sudo -E apt-get update
sudo -E apt-get upgrade -y
sudo -E apt-get install -y "$ros_pkg" ros-dev-tools \
    ros-$distro-mavros ros-$distro-mavros-extras ros-$distro-cv-bridge
sudo /opt/ros/$distro/lib/mavros/install_geographiclib_datasets.sh

echo "Done. Add this to ~/.bashrc:  source /opt/ros/$distro/setup.bash"
