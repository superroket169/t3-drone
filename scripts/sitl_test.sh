#!/bin/bash
# Flies task 1 in ArduCopter SITL and prints PASS or FAIL.
# Run from the workspace root after `colcon build`, inside the ROS environment.
# Usage: scripts/sitl_test.sh [timeout_seconds]
set -o pipefail

timeout_s="${1:-300}"
ardupilot="${ARDUPILOT_DIR:-$HOME/ardupilot}"
sitl_bin="$ardupilot/build/sitl/bin/arducopter"
work=$(mktemp -d)
log="$work/launch.log"

if [[ ! -x "$sitl_bin" ]]; then
    echo "FAIL: SITL binary not found at $sitl_bin (set ARDUPILOT_DIR)"
    exit 1
fi

source /opt/ros/humble/setup.bash
source install/setup.bash

cleanup() {
    [[ -n "${launch_pid:-}" ]] && kill -INT -- -"$launch_pid" 2>/dev/null
    [[ -n "${sitl_pid:-}" ]] && kill "$sitl_pid" 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT

# SITL sends MAVLink to udp:14550, where MAVROS listens
(cd "$work" && exec "$sitl_bin" --model + --speedup 1 -I0 \
    --home 39.92,32.85,900,0 --serial0 udpclient:127.0.0.1:14550 \
    --defaults "$ardupilot/Tools/autotest/default_params/copter.parm") > "$work/sitl.log" 2>&1 &
sitl_pid=$!
sleep 3

setsid ros2 launch bringup task1.launch.xml fcu_url:=udp://:14550@ > "$log" 2>&1 &
launch_pid=$!

start=$SECONDS
last_mode_request=0
while (( SECONDS - start < timeout_s )); do
    if grep -q "Land -> Done" "$log"; then
        echo "PASS: task 1 finished in $(( SECONDS - start ))s"
        exit 0
    fi
    if ! kill -0 "$launch_pid" 2>/dev/null; then
        break
    fi
    # Switch to GUIDED until the node starts; SITL rejects it until the EKF is ready
    if ! grep -q "WaitStart -> Arm" "$log" && (( SECONDS - last_mode_request >= 5 )); then
        last_mode_request=$SECONDS
        timeout 12 ros2 service call /mavros/set_mode mavros_msgs/srv/SetMode \
            "{custom_mode: GUIDED}" > /dev/null 2>&1
    fi
    sleep 1
done

echo "FAIL: task 1 did not finish within ${timeout_s}s. Last phases:"
grep -E "\[task1\]" "$log" | tail -5
echo "Logs: $work"
trap - EXIT
cleanup
exit 1
