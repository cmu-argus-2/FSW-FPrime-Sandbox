## What this adds

<!-- One or two sentences. Link the Python file this ports, if any. -->

## Layer

<!-- See "What a PR touches" in README.md. Pick one. -->

- [ ] **Driver**: `Drivers/<Chip>/`, its instance in `Subtopologies/<Subsystem>/`, bench wiring. Does not touch `Deployments/Argus/`.
- [ ] **Task**: `Tasks/<Task>/`, its subtopology wiring, imported into `Deployments/Argus/`.
- [ ] **Infrastructure**: `Base`, `Board`, `config/`, CI.

## Testing

- [ ] Unit tests pass (`fprime-util check` in the component directory)
- [ ] Flashed to the flatsat. Deployment used:
<!-- Paste a GDS screenshot or event log showing it working on hardware. -->
