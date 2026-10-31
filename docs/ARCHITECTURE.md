# Architecture

The competition has two tasks, each flown as a separate flight. Every flight is
fully autonomous, from takeoff to landing. The RC pilot only takes over in an
emergency.

- **Task 1:** fly a figure-eight around two poles at given coordinates, twice,
  then return and land.
- **Task 2:** search the field with the camera for a 1x1 m red square and a
  2x2 m blue square, drop the red and blue payloads onto them one at a time,
  then return and land.

## Hardware links

```
                      telemetry radio (MAVLink)
   Ground station  <------------------------------------+
   (Mission Planner)                                    |
        ^                                               |
        | WiFi: annotated video                         |
        |                                               |
   +----+------------+   UART (MAVLink)   +-------------+--+
   | Jetson Orin Nano |<----------------->|  Orange Cube   |
   |  ROS 2 + MAVROS  |                   |  ArduCopter    |
   +------------------+                   +---+--------+---+
        ^                                     |        |
        | CSI or USB                  servo outputs   motors / ESCs
     camera                           (payload release)
                                              ^
                                         RC receiver
```

- **Telemetry does not pass through the Jetson.** The radio is wired straight
  to the Cube. If the Jetson crashes, the telemetry link stays up.
- **The Cube flies the drone.** Stabilization, position control and failsafes
  run in ArduCopter. The Jetson only sends high-level commands: mode, arm,
  takeoff, target positions and servo commands.
- **The Cube drives the payload servos.** The Jetson asks for a release with a
  `MAV_CMD_DO_SET_SERVO` command over MAVROS. The Jetson has no GPIO wiring.

## Packages

| Package | Contents |
|---|---|
| `interfaces` | `Detection.msg`, shared by `vision` and `mission` |
| `vision` | `detector` node: finds the red and blue squares and publishes the annotated video |
| `mission` | `task1` and `task2` nodes: flight logic over MAVROS |
| `bringup` | `task1.launch.xml` and `task2.launch.xml`, plus YAML config for each task |

`mavros` and the camera driver are external packages, started from the launch
files.

## Nodes and topics

```
camera driver --/camera/image_raw--> detector --/vision/detections-------> task2
                                         |
                                         +-----/vision/image_annotated--> video stream to ground station

mavros --/mavros/state, /mavros/local_position/pose, /mavros/global_position/global--> task1, task2, detector
task1, task2 --setpoints, services--> mavros
```

### detector (`vision`)

- **Subscribes to:**
  - `/camera/image_raw` (`sensor_msgs/Image`)
  - `/mavros/local_position/pose`, used to project a pixel onto the ground
- **Publishes:**
  - `/vision/detections` (`interfaces/Detection`)
  - `/vision/image_annotated` (`sensor_msgs/Image`), the camera image with the detections drawn on it
- **Detection:** color thresholding plus a square-shape check. The thresholds
  live in `task2.yaml`.
- **Ground position:** computed from the pixel center, the drone pose and the
  camera intrinsics. The camera is assumed to point straight down.

### task1 (`mission`)

The `task1` node steps through these phases:

```
WaitStart -> Arm -> Takeoff -> Climb -> FollowRoute -> Land -> Done
```

- **Poles:** given as launch arguments, either as `[east, north]` meters from
  the takeoff point (`coord_frame:=local`) or as `[latitude, longitude]`
  (`coord_frame:=gps`). GPS coordinates are converted to local meters around
  the takeoff point.
- **Figure-eight:** two tangent circles, one around each pole, each with a
  radius of half the pole distance. They touch at the midpoint between the
  poles. The first circle is flown counterclockwise, the second clockwise.
  Each circle is split into `points_per_circle` waypoints.
- **Route:** the drone flies to the figure-eight waypoint nearest to the
  takeoff point, flies `laps` full figure-eights from there, ends at the same
  waypoint, then flies back above the takeoff point.
- **Following:** waypoints are sent one at a time as local position setpoints.
  The next one is sent once the drone is within `reach_tolerance` of the
  current one.
- **Landing:** after the last waypoint, the node switches to ArduPilot's
  `LAND` mode.

### task2 (`mission`)

The `task2` node steps through these phases:

```
WaitStart -> Arm -> Takeoff -> Climb -> Search
  -> Approach(red) -> Drop(red) -> Approach(blue) -> Drop(blue) -> Return -> Land -> Done
```

- **Search:** the node flies a lawnmower pattern over the search area defined
  in `task2.yaml`, and stores each target's ground position the first time it
  is detected.
- **Approach:** the node flies above a stored target and centers over it,
  using fresh detections at a lower altitude.
- **Drop:** the node sends a `MAV_CMD_DO_SET_SERVO` command with the servo
  channel for that payload.

## Rules shared by both mission nodes

- **GUIDED starts the flight.** After launch the node waits on the ground. It
  arms and takes off only when the operator switches the Cube to GUIDED.
- **Only command in GUIDED.** If `/mavros/state` shows any other mode, for
  example because the pilot took over, the node stops sending commands and
  waits. It must never fight the pilot.
- **Land with ArduPilot.** The final landing uses ArduPilot's `LAND` mode, so
  the Cube finishes it on its own.
- **Ask for telemetry streams.** ArduPilot sends position and attitude data
  only after it is asked. Each node calls `mavros/set_stream_rate` once MAVROS
  is connected, and again after every reconnect.
- **Use relative topic names in code** (`mavros/state`, not `/mavros/state`),
  so a launch file can move everything under a namespace.
- **Use `rclcpp::SensorDataQoS()`** when subscribing to MAVROS sensor topics.

## MAVROS

The launch files start MAVROS with only the plugins the nodes use, listed in
`src/bringup/config/mavros_plugins.yaml`. The full ArduPilot plugin set
overloads the CPU, and MAVROS then loses the heartbeat and drops the link.
Add a plugin to that list before using any topic or service it provides.

## Open decisions

- TODO: camera model and its ROS driver (the Orin has `/dev/video0` and `/dev/video1`; CSI or USB is unknown)
- TODO: Jetson UART device and baud rate (`fcu_url` in the launch files)
- TODO: how the annotated video reaches the ground station (e.g. GStreamer H.264 over UDP)
- TODO: payload servo output channels and PWM values for open/closed
- TODO: what the drone does if the Jetson dies in flight (GUIDED keeps hovering; the pilot must take over)
