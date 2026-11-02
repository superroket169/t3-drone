# t3-drone

ROS 2 Jazzy software for the TEKNOFEST high school rotary-wing UAV competition.
The software runs on a Jetson Orin Nano, which talks to an Orange Cube running
ArduCopter through MAVROS. The tasks and the system layout are described in
[ARCHITECTURE.md](ARCHITECTURE.md).

## Layout

```
scripts/         setup and SITL test scripts
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

## Run on the drone

### Task 1

```bash
ros2 launch bringup task1.launch.xml coord_frame:=gps \
    pole1:="[39.123456, 32.654321]" pole2:="[39.123500, 32.654400]"
```

| Argument | Meaning |
|---|---|
| `coord_frame` | `local`: poles are `[east, north]` in meters from the takeoff point. `gps`: poles are `[latitude, longitude]` in degrees. |
| `pole1`, `pole2` | Pole positions. Always write decimals (`[0.0, 10.0]`, not `[0, 10]`), or the node exits with a type error. |
| `fcu_url` | MAVLink link to the Orange Cube. The default is the Jetson UART. |

Altitude, laps, points per circle and waypoint tolerance are in
`src/bringup/config/task1.yaml`.

After launch, the node waits on the ground. The flight starts when the operator
switches the Cube to **GUIDED**, from the RC transmitter or Mission Planner.
Switching to any other mode during the flight pauses the node, and the pilot
takes over.

### Task 2

```bash
ros2 launch bringup task2.launch.xml
```

TODO: write the task 2 and detector nodes; this launch file only starts MAVROS

## Simulation (SITL)

SITL runs the real ArduCopter firmware on your computer, with simulated
physics. Every flight change must pass the SITL test before it flies on the
drone.

### Install (once)

```bash
sudo apt-get install -y git g++ make python3-dev python3-empy python3-future \
    python3-lxml python3-pexpect
git clone --depth 1 --branch Copter-4.7.2 --recurse-submodules --shallow-submodules \
    https://github.com/ArduPilot/ardupilot.git ~/ardupilot
cd ~/ardupilot
./waf configure --board sitl
./waf copter
```

The build takes about 15 minutes and uses 1.5 GB of disk.

### Test

From the workspace root, after `colcon build`:

```bash
scripts/sitl_test.sh
```

The script starts SITL, MAVROS and the task 1 node, switches to GUIDED and
waits for the flight to finish. It prints `PASS` or `FAIL` and takes about
2.5 minutes. If ArduPilot is not in `~/ardupilot`, set `ARDUPILOT_DIR`.

### Fly by hand

```bash
# terminal 1: SITL, sending MAVLink to udp:14550
cd "$(mktemp -d)" && ~/ardupilot/build/sitl/bin/arducopter --model + -I0 \
    --home 39.92,32.85,900,0 --serial0 udpclient:127.0.0.1:14550 \
    --defaults ~/ardupilot/Tools/autotest/default_params/copter.parm

# terminal 2: MAVROS and the task 1 node
ros2 launch bringup task1.launch.xml fcu_url:=udp://:14550@

# terminal 3: start the flight, once SITL has a GPS fix (about 40 s)
ros2 service call /mavros/set_mode mavros_msgs/srv/SetMode "{custom_mode: GUIDED}"
```

## Useful commands

```bash
ros2 topic list                  # all topics
ros2 topic echo /mavros/state    # print messages on a topic
ros2 topic hz /camera/image_raw  # message rate
ros2 node list                   # running nodes
```

## License

Licensed under either of [Apache License, Version 2.0](../LICENSE-APACHE) or
[MIT license](../LICENSE-MIT), at your option.
