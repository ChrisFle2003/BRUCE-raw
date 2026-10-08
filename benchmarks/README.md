# BRUCE benchmark results

[Analysis of the first remote release run](../docs/implementation/152-benchmark-v1-results.md)

- [Standard: 10 repetitions per case](2026-09-25-standard-v1/report.md)
- [Stress: 30 repetitions per case](2026-09-25-stress-v1/report.md)
- [Run the suite and understand its measurement limits](../docs/cli/benchmark.md)

Only these three compact artifacts are versioned for each run: `report.md`,
`summary.csv`, and `manifest.json`. Raw data, intermediate state, and new
measurement directories remain local and are not included in a distributed CLI
archive. New runs use new directories. Do not form a ranking from different
tasks, stop reasons, or hardware environments.
