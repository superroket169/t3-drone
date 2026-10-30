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
- Keep each node in a single `.cpp` file inside its package's `src/` folder.
- Write topic and service names without a leading slash (`mavros/state`, not
  `/mavros/state`).
- Subscribe to MAVROS sensor topics with `rclcpp::SensorDataQoS()`. Otherwise
  no messages arrive and no error is shown.
- Put tunable values (altitudes, coordinates, thresholds) in the
  `bringup/config/*.yaml` files, never in the code.
- Mark unfinished work with `TODO:` and a short note.

## Testing

Test every flight change in SITL before flying it on the real drone.
See [README.md](README.md#run).
