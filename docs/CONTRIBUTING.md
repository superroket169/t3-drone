# Contributing

## Workflow

1. Pick an issue and assign it to yourself.
2. Create a branch from `main`, named after the issue number:

   ```bash
   git switch -c feat/12-task1-figure-eight
   ```

   Use `feat/` for new features, `fix/` for bug fixes and `docs/` for
   documentation.
3. Commit in small steps.
4. Before opening a pull request, make sure the workspace builds cleanly:

   ```bash
   colcon build --symlink-install
   ```

5. Open a pull request to `main` and write `Closes #12` in its description.
   Nothing is pushed to `main` directly.

## Commit messages

Write one line in the form `type(scope): what changed`, in English:

```
feat(mission): task 1 figure-eight waypoints
fix(vision): ignore detections smaller than the minimum area
docs: camera setup on the Jetson
```

- **Types:** `feat`, `fix`, `docs`, `chore`
- **Scopes:** the package name (`interfaces`, `vision`, `mission`, `bringup`)
  or `scripts`

## Code

- Write C++17, using `rclcpp`.
- Indent with 4 spaces.
- Keep each node in a single `.cpp` file inside its package's `src/` folder,
  `main` included. Split the work into small functions; the timer callback
  only picks the current phase and calls them.
- Write topic and service names without a leading slash (`mavros/state`, not
  `/mavros/state`).
- Subscribe to MAVROS sensor topics with `rclcpp::SensorDataQoS()`. Otherwise
  no messages arrive and no error is shown.
- Put tunable values (altitudes, tolerances, thresholds) in the
  `bringup/config/*.yaml` files, never in the code. Values that change on
  every flight, like pole coordinates, are launch arguments.
- Before using a new MAVROS topic or service, add its plugin to
  `bringup/config/mavros_plugins.yaml`.
- Mark unfinished work with `TODO:` and a short note.

## Testing

Every flight change must pass `scripts/sitl_test.sh` before it flies on the
real drone. See [README.md](README.md#simulation-sitl).
