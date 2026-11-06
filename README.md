<img src="https://github.com/brusale/jetlib/blob/master/docs/jetlib.png" height="125">

Generate a sample of particles inside a jet. Jetlib generates events through the Pythia8 event generator and clusters particles with FastJet, producing a ROOT file containing information about jets and their constituents.

Jetlib uses Pythia 8.306, FastJet 3.5.0 and ROOT. Jets are clustered with anti-kT, R = 0.4.

## Building

The DICE container provides all dependencies (FastJet in `/opt/fastjet-3.5.0`, the default).

```bash
git clone <repo> jetlib && cd jetlib
git submodule update --init      # pybind11, needed for the Python module
mkdir build && cd build
cmake ..
make -j8
```

CMake options:

| Option | Meaning | Default |
|---|---|---|
| `-DFASTJET_DIR=<prefix>` | FastJet installation (contains `bin/fastjet-config`); otherwise `fastjet-config` is taken from `PATH` | `/opt/fastjet-3.5.0` |
| `-DJETLIB_PYTHON=OFF` | build only the `jetlib` executable, without Python/pybind11 | `ON` |

## Running

```bash
./jetlib -c ../config/ttbar.cfg -f output.root -n 1000
```

`-c` is the Pythia config file. Every non-empty line that does not start with `#` is passed to Pythia, and a line Pythia does not accept stops the run. Without `-c`, `config/ttbar.cfg` of the source tree jetlib was built from is used. The config file in use is printed at startup. The repository has `config/ttbar.cfg` (tt̄), `config/zprime.cfg` (300 GeV Z′ → u, d, s, c quarks) and `config/tau.cfg` (Z → ττ). `-n` is the number of events, `-f` the output file, `-d` prints every event. `./jetlib --help` lists all options.

## Particles that are clustered

Only visible final-state particles enter the jet clustering:

- Neutrinos and other invisible particles (Pythia `isVisible() == false`) are always removed.
- Prompt leptons are removed by default. These are final-state electrons and muons without a hadron among their ancestors, for example from W, Z or τ decays. Leptons from hadron decays (e.g. semileptonic b decays) stay in the jets. Use `--keep-prompt-leptons` to cluster prompt leptons too.
- Undecayed unstable particles are always removed. Rarely, a Pythia decay fails (`PYTHIA Error in ParticleDecays::decay: failed to find workable decay channel`) and the particle stays in the final state, e.g. an f₀(1370) (PDG 10221) whose mass was drawn below the thresholds of most of its decay channels. Detector simulations such as Geant4 do not know these particles. jetlib removes every final-state particle that Pythia should have decayed (`mayDecay()` and a decay table). The jets therefore contain only particles that are stable in Pythia's default settings (γ, e, μ, π±, K±, K⁰_L, p, n and antiparticles). Caveat: if the config leaves particles undecayed on purpose (`ParticleDecays:limitTau0`, `limitTau`, `limitRadius`, `limitCylinder`, or `HadronLevel:Decay = off`), e.g. K⁰_S or Λ, these are removed as well. The check is `checks/check_undecayed.py`.

The numbers of removed particles are printed at the end of the run (undecayed particles per PDG ID). In Python: `EventGenerator.setKeepPromptLeptons(True)`, `getNRemovedUndecayed()`, `getRemovedUndecayed()`.

## Event content (underlying event, pileup)

The events are Pythia's defaults apart from the config file: initial- and final-state radiation, multiparton interactions (MPI) and hadronization are on. Jets therefore contain underlying-event particles. There is no pileup. To change this, add Pythia settings to the config file, for example:

```
PartonLevel:MPI = off      # no multiparton interactions (no underlying event)
PartonLevel:ISR = off      # no initial-state radiation
```

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

## Output

One jet per entry of the `JetTree` tree. Per-jet kinematics: `jetPt`, `jetEta`, `jetPhi`, `jetEnergy`, `jetNConstituents`. Constituents: `jetConstituentsPx/Py/Pz/Pt/E/Eta/Phi/PdgId`. The branch `jetConstituentsEta` was called `jetConsituentsEta` (misspelt) before; readers of older files need the old name. Bookkeeping:

- `eventIndex`: index of the generated event in this run (0-based). Jets with the same `eventIndex` come from the same Pythia event.
- `jetIndex`: pT rank of the jet among the selected jets of its event (0 = hardest).

Provenance is stored as `TNamed` objects (the value is the title) in the directory `jetlib/`: `jetlib_version` (git describe of the build), `command_line`, `config_file`, `config` (full text), `seed`, `pythia_version`, `fastjet_version`, `jet_algorithm`, `cluster_pt_min`, `selection`, `particles`, and the counters `n_events_requested`, `n_events_accepted`, `n_failed_next`, `n_removed_invisible`, `n_removed_prompt_leptons`, `n_removed_undecayed` (total, then counts per PDG ID) and `n_jets_written`. With uproot: `uproot.open(f)["jetlib/seed"].member("fTitle")`.

Jets are written event by event, so consecutive entries are correlated. Shuffle them, or sample by `eventIndex`, if that matters.

## Random seed

`--seed <int>` sets the Pythia seed and always overrides `Random:seed` in the config file. Allowed values are 0 to 900000000 (Pythia's maximum). Negative values are rejected; Pythia would map them to its fixed default seed.

The default is `0`. jetlib then draws a seed in [1, 900000000] from `std::random_device`, mixed with the time in nanoseconds and the process ID, and passes it to Pythia. (Pythia's own time-based seed for 0 has a resolution of one second, so jobs started together produced identical events.) The same happens when no `--seed` is given in Python and the config has `Random:seed = 0`. The seed actually used is printed at startup and stored in the output as `jetlib/seed`. Rerunning with `--seed <that value>` reproduces the file.

For planned production you can still give every job its own seed explicitly:

```bash
./jetlib -c ../config/ttbar.cfg -f out_17.root -n 1000 --seed 17
```

In Python: `EventGenerator.setSeed(seed)` before `initialize()`; `getSeed()` returns the seed actually used.

## Python module

`make` also builds `jetlibpy` in `build/` (unless `-DJETLIB_PYTHON=OFF`). Run it in the same environment (container) it was built in:

```python
import sys; sys.path.insert(0, "build")
import jetlibpy

gen = jetlibpy.EventGenerator("config/ttbar.cfg")
gen.setSeed(17)                  # before initialize(); 0 = jetlib draws a seed
gen.initialize()
gen.generateEvents(100)          # keeps all jets in memory
for event in gen.getJetCollection():
    for jet in event:
        print(jet.pt(), jet.eta(), jet.phi(), len(jet.constituents()))

ana = jetlibpy.JetAnalyzer("out.root")
sel = jetlibpy.JetSelection(); sel.ptMin = 30; sel.etaMin = -3; sel.etaMax = 3
ana.setSelection(sel)
ana.analyze(gen.getJetCollection())   # same tree as the CLI writes (no provenance unless setMetadata is used)
```

- `phi()` is in [−π, π), as in the output tree; `phi_02pi()` gives [0, 2π).
- Jets keep their FastJet `ClusterSequence` alive, so `constituents()` works as long as the jet exists. FastJet errors arrive as `RuntimeError`.
- `JetAnalyzer.setFilter(expr)` adds a string filter on top of the selection, e.g. `"pt > 30 && (abs(eta) < 2.5 || e > 500)"`. Variables: `pt`, `eta`, `phi`, `e`/`energy`, optionally in `abs()`; operators `> < >= <= == !=`, `&&`, `||` and parentheses. An invalid filter raises `ValueError`.
- `JetAnalysis` clusters your own particles: `setParticles([jetlibpy.PseudoJet(px, py, pz, E), ...])`, then `clusterJets(R, ptMin)` and `getJets(ptMin)`.
