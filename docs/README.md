# t3-drone

ROS 2 Jazzy software for the TEKNOFEST high school rotary-wing UAV competition.
The software runs on a Jetson Orin Nano, which talks to an Orange Cube running
ArduCopter through MAVROS. The tasks and the system layout are described in
[ARCHITECTURE.md](ARCHITECTURE.md).

## Layout

```
scripts/         setup scripts
docs/            README, ARCHITECTURE, CONTRIBUTING
src/interfaces/  message definitions
src/vision/      square detection and video
src/mission/     task 1 and task 2 flight logic
src/bringup/     launch files and config
```

## Setup

ROS 2 Jazzy needs Ubuntu 24.04.

### Ubuntu 24.04 or Jetson (JetPack 7)

```bash
scripts/setup.sh              # Jetson: no GUI tools
scripts/setup.sh --desktop    # dev machine: with RViz and GUI tools
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

### Other Linux distributions

Run Ubuntu 24.04 in a [distrobox](https://distrobox.it/) container. The
container shares your home directory and network with the host.

```bash
distrobox create --name ros-jazzy --image docker.io/library/ubuntu:24.04
distrobox enter ros-jazzy
scripts/setup.sh --desktop
```

## Build

```bash
cd t3-drone
colcon build --symlink-install
source install/setup.bash
```

Source `install/setup.bash` again in every new terminal.

## Run

On the drone:

```bash
ros2 launch bringup task1.launch.xml
ros2 launch bringup task2.launch.xml
```

Against the ArduPilot simulator (SITL), started with `sim_vehicle.py -v ArduCopter`:

```bash
ros2 launch bringup task1.launch.xml fcu_url:=udp://:14550@
```

TODO: add SITL installation steps

## Useful commands

```bash
ros2 topic list                  # all topics
ros2 topic echo /mavros/state    # print messages on a topic
ros2 topic hz /camera/image_raw  # message rate
ros2 node list                   # running nodes
```
