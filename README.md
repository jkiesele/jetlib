<img src="https://github.com/brusale/jetlib/blob/master/docs/jetlib.png" height="125">

Generate a sample of particles inside a jet. Jetlib generates events through the Pythia8 event generator and clusters particles with FastJet, producing a ROOT file containing information about jets and their constituents.

To run jetlib, clone the repo and follow these steps

```bash
cd jetlib
mkdir build && cd build
cmake ..
make -jN
./jetlib -f output.root --nEvents 5
```
Jetlib uses `fastjet-3.5.0` and `pythia8.306`

## Particles that are clustered

Only visible final-state particles enter the jet clustering:

- Neutrinos and other invisible particles (Pythia `isVisible() == false`) are always removed.
- Prompt leptons are removed by default. These are final-state electrons and muons without a hadron among their ancestors, for example from W, Z or τ decays. Leptons from hadron decays (e.g. semileptonic b decays) stay in the jets. Use `--keep-prompt-leptons` to cluster prompt leptons too.

The numbers of removed particles are printed at the end of the run. In Python: `EventGenerator.setKeepPromptLeptons(True)`.

## Jet selection

By default jetlib writes jets with `pt > 30` GeV and `-3 < eta < 3`. The selection can be changed on the command line. All cuts are strict (`> min`, `< max`).

| Option | Meaning | Default |
|---|---|---|
| `--pt-min <GeV>` | minimum jet pT, must be >= 0 | 30 |
| `--pt-max <GeV>` | maximum jet pT | none |
| `--eta-min <value>` | minimum jet η | −3 (none with `--abs-eta`) |
| `--eta-max <value>` | maximum jet η | 3 |
| `--abs-eta` | apply the η cuts to \|η\| | off (signed η) |

Setting only one η bound keeps the default for the other: `--eta-min 1.7` selects 1.7 < η < 3. With `--abs-eta` alone, the selection is |η| < 3. The selection before this option existed (η > 1.7, no upper limit) can be approximated with `--eta-min 1.7 --eta-max 100`.

The clustering keeps jets above `min(20 GeV, --pt-min)`, so it never cuts harder than the selection. Inconsistent cuts (min ≥ max, negative pT, negative η bounds with `--abs-eta`) are rejected with an error. The active selection is printed at startup.

Example: both forward regions, 1.7 < |η| < 2.6, pT > 30 GeV:

```bash
./jetlib -c ../config/ttbar.cfg -f out.root -n 1000 --abs-eta --eta-min 1.7 --eta-max 2.6 --pt-min 30
```

In Python the same selection is a `jetlibpy.JetSelection` passed to `JetAnalyzer.setSelection`.

## Random seed

`--seed <int>` sets the Pythia seed and always overrides `Random:seed` in the config file. The default is `0`, which Pythia turns into a time-based seed, so two runs differ. Allowed values are 0 to 900000000 (Pythia's maximum). Negative values are rejected; Pythia would map them to its fixed default seed. The seed is printed at startup.

The time-based seed has a resolution of one second: jobs started in the same second produce identical events. For reproducible or parallel production, give every job its own non-zero seed:

```bash
./jetlib -c ../config/ttbar.cfg -f out_17.root -n 1000 --seed 17
```

In Python: `EventGenerator.setSeed(seed)` before `initialize()`.
