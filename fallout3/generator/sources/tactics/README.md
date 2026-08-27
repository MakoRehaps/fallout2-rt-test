# Fallout Tactics Source Adapter

Fallout Tactics is an optional DLC source library for the Fallout 3 classic generator.

The engine should **not** attempt to load native Tactics map files. The adapter's job is to extract reusable layout ideas and structure vocabulary into the generator's neutral prefab representation.

Target categories:

- fortified wall runs and checkpoints
- Brotherhood/military compounds
- bunkers
- industrial yards
- warehouses
- large tactical battle spaces
- vehicle bays / road blocks
- trenches and defensive positions
- machinery clusters
- large interior room arrangements

Pipeline:

```text
Tactics data
  -> inventory/index
  -> source-specific parser
  -> neutral prefab/layout records
  -> style/tag filtering
  -> rebuild with classic-engine-compatible tiles/objects
  -> native Fallout 2 MAP output
```

No Tactics asset is assumed to be binary-compatible with fallout2-rt-test.
