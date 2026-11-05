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

## Jet selection

By default jetlib writes jets with `pt > 30` GeV and signed `eta > 1.7`, with no upper limits. The selection can be changed on the command line. All cuts are strict (`> min`, `< max`).

| Option | Meaning | Default |
|---|---|---|
| `--pt-min <GeV>` | minimum jet pT, must be >= 0 | 30 |
| `--pt-max <GeV>` | maximum jet pT | none |
| `--eta-min <value>` | minimum jet η | 1.7 |
| `--eta-max <value>` | maximum jet η | none |
| `--abs-eta` | apply the η cuts to \|η\| | off (signed η) |

The clustering keeps jets above `min(20 GeV, --pt-min)`, so it never cuts harder than the selection. Inconsistent cuts (min ≥ max, negative pT, negative η bounds with `--abs-eta`) are rejected with an error. The active selection is printed at startup.

Example: both forward regions, 1.7 < |η| < 2.6, pT > 30 GeV:

```bash
./jetlib -c ../config/ttbar.cfg -f out.root -n 1000 --abs-eta --eta-min 1.7 --eta-max 2.6 --pt-min 30
```

In Python the same selection is a `jetlibpy.JetSelection` passed to `JetAnalyzer.setSelection`.
