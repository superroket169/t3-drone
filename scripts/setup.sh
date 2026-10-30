#!/bin/bash
# Installs ROS 2 Humble, MAVROS and build tools on Ubuntu 22.04 (Jetson or dev machine).
# Usage: scripts/setup.sh            -> ros-base (Jetson)
#        scripts/setup.sh --desktop  -> desktop with RViz and GUI tools (dev machine)
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

ros_pkg=ros-humble-ros-base
if [[ "${1:-}" == "--desktop" ]]; then
  ros_pkg=ros-humble-desktop
fi

codename=$(. /etc/os-release && echo "$UBUNTU_CODENAME")
if [[ "$codename" != "jammy" ]]; then
  echo "ROS 2 Humble needs Ubuntu 22.04 (jammy), found: $codename" >&2
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
  ros-humble-mavros ros-humble-mavros-extras ros-humble-cv-bridge
sudo /opt/ros/humble/lib/mavros/install_geographiclib_datasets.sh

echo "Done. Add this to ~/.bashrc:  source /opt/ros/humble/setup.bash"
