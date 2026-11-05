# jetlib review

Scope: all of `jetlib.cc`, `interface/`, `src/`, `bind/bindings.cc`, `config/`, `CMakeLists.txt`, `cmake/`, and `README.md` at master `7bc2c3b`. The repository was not changed in this phase.

## How the findings were checked

- Built in the DICE container (Pythia 8.306, FastJet 3.5.0, ROOT 6.38). The build is clean, with no compiler warnings, and produces both `jetlib` and `jetlibpy`.
- Experiments ran under `/ceph/jkiesele/temp_data/jetlib/review/`. The reference sample is `ttbar.cfg` with `Random:seed = 12345` (a copy of the config) and 4000 events, giving 3026 jets after the default selection.
- Quantities marked *measured* come from these runs. Everything else comes from reading the code.

**Library impact** says whether the finding changes the ROOT file that DICE uses as a jet library with the current CLI defaults: **yes**, **conditional** (only for some inputs or usage), or **no**.

---

## 1. Correctness of the produced jet library

### 1.1 Hardcoded selection writes only forward, positive-η jets
- **Location:** `jetlib.cc:46` (`setFilter("eta > 1.7 && pt > 30")`)
- **Problem:** η is cut on its signed value, so the negative-η hemisphere is dropped entirely, and nothing on the command line can change the cut. There is also no upper η limit. *Measured:* of 3026 jets, 0 have η < 0, and η reaches 5.43, with 78 jets above η = 4. That is likely outside the DICE detector acceptance.
- **Library impact:** yes. Half the phase space is missing, and the library contains very forward jets.
- **Fix:** Phase 2 change A, which adds CLI cuts, `--abs-eta`, and upper limits.
- **Confidence:** high (measured).

### 1.2 Neutrinos are clustered, which creates fake, mostly invisible "jets"
- **Location:** `src/JetAnalysis.cc:5`. Only `isFinal()` is checked.
- **Problem:** neutrinos enter the clustering, so they shift jet axes and add to `jetPt` and `jetEnergy`. In ttbar, W→ℓν neutrinos form "jets" of their own. *Measured:* 22.9% of jets contain at least one neutrino, and **8.2% of jets carry more than 90% of their energy in neutrinos**. 11.3% of the selected jets have a visible pT below the 30 GeV cut. DICE shoots these neutrinos as primaries, and they deposit nothing.
- **Library impact:** yes. The jet kinematics are biased and the library contains fake jets.
- **Fix:** skip invisible particles before clustering: `if (!event[i].isFinal() || !event[i].isVisible()) continue;`. `isVisible()` also covers invisible BSM particles. Apply the pT selection to visible jets only.
- **Confidence:** high (measured).

### 1.3 Prompt leptons become jets
- **Location:** `src/JetAnalysis.cc:3-10`
- **Problem:** isolated electrons and muons from W/Z/τ decays are clustered like hadrons, so a single lepton can form a "jet". *Measured:* in 5.3% of jets, one e/μ carries more than 50% of the energy.
- **Library impact:** yes, if the library is meant to contain hadronic jets only. Whether that matters is a design question.
- **Fix:** make the content explicit. Either exclude prompt leptons (final-state e/μ whose mother chain leads to W, Z or τ, without going through a hadron), or store a per-jet lepton-fraction or flag so DICE can select jets.
- **Confidence:** medium. The numbers are measured; whether this is a problem depends on the intended use.

### 1.4 Pythia init failures and config typos produce an empty file and exit 0
- **Location:** `interface/EventGenerator.h:25` (return value of `readString` ignored), `:27` (return value of `init()` ignored), `:32`
- **Problem:** in Pythia 8.306, an unknown setting makes `init()` fail. Every `next()` then fails, and each failure is skipped silently. *Measured:* a config with the typo `Top:gg2ttbarr` and a config with `Beams:eCM = -5` both wrote a file with 0 jets and exited with **status 0**.
- **Library impact:** conditional. A batch job can produce an empty library without any visible error.
- **Fix:** check `readString()` and `init()`, and throw on failure. Exit non-zero if no events were accepted.
- **Confidence:** high (measured).

### 1.5 Time-based seed, no CLI override, and the seed is not recorded
- **Location:** `config/*.cfg:3` (`Random:seed = 0`), `interface/ArgumentParser.h`
- **Problem:** every run uses a different seed, which is not stored anywhere. Parallel jobs started in the same second can get the same seed, and outputs cannot be reproduced. *Measured:* with a fixed seed in the config, master is reproducible: two runs of 1000 events have identical tree contents.
- **Library impact:** yes, for reproducibility. Duplicate seeds in parallel jobs would also put duplicate jets in a merged library.
- **Fix:** Phase 2 change B (`--seed`). Also store the seed and config text in the output file (e.g. a `TNamed`). That is not part of Phase 2.
- **Confidence:** high.

### 1.6 Clustering pT threshold of 20 GeV hardcoded twice
- **Location:** `src/JetAnalysis.cc:15` (`inclusive_jets(20.0)`), `interface/EventGenerator.h:37` (`getJets(20.0)`)
- **Problem:** no selection below 20 GeV can work. The second cut is redundant, since `inclusive_jets` already applies pT ≥ 20.
- **Library impact:** no with the current defaults (pT > 30). It does affect any selection with pT < 20.
- **Fix:** pass the threshold through as a parameter. Phase 2 change A does this.
- **Confidence:** high.

### 1.7 Failed `pythia.next()` calls are skipped silently
- **Location:** `interface/EventGenerator.h:32`
- **Problem:** the run produces fewer events than requested and does not report the shortfall. *Measured:* 0 failures in 4000 ttbar events, so the effect is small in normal running. With a broken init (1.4), every event fails.
- **Library impact:** conditional (small).
- **Fix:** count the failures and print them at the end. Either loop until `nEvents` events are accepted, or abort after N consecutive failures.
- **Confidence:** high.

### 1.8 Consecutive jets come from the same event, and there is no event index
- **Location:** `src/JetAnalyzer.cc:185-207`, `setBranches`
- **Problem:** jets are written in event order, and there is no `eventIndex` or `jetIndex`. Jets from the same event cannot be identified. If DICE reads entries sequentially, neighbouring jets are correlated (e.g. two b-jets from the same tt̄ event).
- **Library impact:** yes. The library lacks metadata, and sequential use gives correlated sampling.
- **Fix:** add `eventIndex` and `jetIndex` (pT rank in the event) branches. DICE could then also shuffle the entries.
- **Confidence:** high.

---

## 2. Robustness and error handling

### 2.1 Unknown or invalid output path segfaults, but only after all events are generated
- **Location:** `jetlib.cc:44` (the analyzer is constructed after generation), `interface/JetAnalyzer.h:13-16` (no `IsZombie()` or null check)
- **Problem:** *Measured:* `-f /nonexistent/dir/x.root` runs the full generation and then segfaults (exit 139) in `Close()` on a null `TFile*`. A long batch job wastes all of its CPU time.
- **Library impact:** no (no file is produced).
- **Fix:** open the output first, check for null or `IsZombie()`, and throw.
- **Confidence:** high (measured).

### 2.2 Unvalidated `--nEvents`
- **Location:** `interface/ArgumentParser.h:23`, `jetlib.cc:34` (`int` → `size_t`)
- **Problem:** *Measured:* `-n -1` wraps around to about 1.8e19 and runs effectively forever while memory grows (see 3.1). `-n abc` throws an uncaught `std::invalid_argument`, which aborts with a core dump (exit 134).
- **Library impact:** no.
- **Fix:** parse with `std::stoll` plus a full-consumption check, require n > 0, and print a clear error.
- **Confidence:** high (measured).

### 2.3 The filter parser accepts invalid expressions silently
- **Location:** `src/JetAnalyzer.cc:102-174`
- **Problem:** *Measured* through `jetlibpy`, on 138 jets:
  - Unknown variable (`Pt>1000`): all jets pass, with one warning printed **per jet**.
  - Single `=` (`pt=1000`): the operator falls through to `return true`, so all jets pass, with no warning.
  - Trailing garbage (`pt>1000abc`): accepted as `pt>1000`.
  - Trailing `&&` (`pt>30&&`): `expr.front()` is called on an empty string, which is undefined behaviour. *Measured:* the process aborts.
  - Unbalanced parentheses and an empty value are not detected.
- **Library impact:** conditional. The CLI uses a fixed, valid filter today, but `setFilter` is exposed in Python.
- **Fix:** validate when the filter is set. Throw on an unknown variable or operator, on an empty term, or on unconsumed characters (use the `std::stod` `pos` argument). Resolve the variable to an enum once, not per jet. After Phase 2 the CLI no longer uses the string filter. Either harden it or remove it.
- **Confidence:** high (measured).

### 2.4 With two `JetAnalyzer`s, the tree goes into the wrong file and is lost
- **Location:** `src/JetAnalyzer.cc:182` (`new TTree` attaches to the current `gDirectory`, not `outputFile_`), `interface/JetAnalyzer.h:16` (`Close()` without `Write()`)
- **Problem:** the tree is attached to whichever file was opened most recently. *Measured* in Python: after `a1 = JetAnalyzer("a1.root"); a2 = JetAnalyzer("a2.root"); a1.analyze(jets)`, both files are empty and the jets are lost without an error. The class is also copyable while owning a raw `TFile*`, so a copy would close the file twice.
- **Library impact:** conditional (Python or multi-output use). The CLI is not affected.
- **Fix:** call `outputFile_->cd()` before `new TTree`, or `tree->SetDirectory(outputFile_)`. Hold the file in a `std::unique_ptr<TFile>` and delete the copy operations.
- **Confidence:** high (measured).

### 2.5 Uncaught exceptions dump core
- **Location:** `jetlib.cc` `main`. Throw sites: `EventGenerator.h:18` (config not found), `JetAnalyzer.cc:122,130,139`, `std::stoi`/`std::stod`
- **Problem:** errors end in `std::terminate`, so the user sees an abort and a core file instead of a message.
- **Library impact:** no.
- **Fix:** wrap `main` in `try/catch`, print the message, and return 1.
- **Confidence:** high.

---

## 3. Resources

### 3.1 All events and `ClusterSequence`s stay in memory
- **Location:** `interface/EventGenerator.h:38-39`, `jetlib.cc:39-47`
- **Problem:** *Measured:* peak RSS is 475 MB at 1000 events and 1203 MB at 4000 events, about **0.24 MB per event**. That extrapolates to about 240 GB for 10⁶ events. The `ClusterSequence`s cannot simply be freed: `JetAnalyzer::analyze` calls `jet.constituents()`, which needs them (see 4.1).
- **Library impact:** no, but it limits the library size per job.
- **Fix:** stream the work. For each event: generate, cluster, select, fill, then drop the `ClusterSequence`. Either give `JetAnalyzer` a per-event `fill(jets, eventIndex)`, or extract the constituents into plain vectors before discarding the sequence.
- **Confidence:** high (measured).

---

## 4. Python bindings

### 4.1 Jets outlive their `ClusterSequence`, giving an opaque error
- **Location:** `bind/bindings.cc:30`
- **Problem:** `getJetCollection()` returns copies of `PseudoJet`s that point into `ClusterSequence`s owned by the `EventGenerator`. *Measured:* after `del gen`, `jet.constituents()` raises `RuntimeError: Caught an unknown exception!`, because `fastjet::Error` is not translated.
- **Library impact:** no.
- **Fix:** register a pybind11 exception translator for `fastjet::Error`. Either use `keep_alive` or return constituents as plain arrays.
- **Confidence:** high (measured).

### 4.2 `JetAnalysis` is half usable from Python
- **Location:** `bind/bindings.cc:36`, `interface/JetAnalysis.h:29-32`, `src/JetAnalysis.cc:3`
- **Problem:** `convertPythiaToPseudoJets` takes a `Pythia8::Event`, which is not bound, so it cannot be called from Python. `convertPythiaToPseudoJets` appends to `particles_` without clearing it, and `clear()` does not reset `clusterSeq_`. Reusing one object therefore accumulates events.
- **Library impact:** no. The CLI creates one `JetAnalysis` per event.
- **Fix:** clear `particles_` at the start of the conversion, or remove the binding.
- **Confidence:** high.

### 4.3 φ convention differs between the binding and the output
- **Location:** `bind/bindings.cc:17` (`phi()`, range [0, 2π)), `src/JetAnalyzer.cc:191,202` (`phi_std()`, range [−π, π)), `jetlib.cc:13,18` (debug print uses `phi()`)
- **Library impact:** no.
- **Fix:** use `phi_std` everywhere.
- **Confidence:** high.

---

## 5. Output format

### 5.1 Branch name typo `jetConsituentsEta`
- **Location:** `src/JetAnalyzer.cc:18`, `interface/JetAnalyzer.h:34,51`
- **Library impact:** no for DICE, which does not read this branch. Other readers are affected.
- **Fix:** rename it to `jetConstituentsEta`. Optionally keep the old name as a `TTree::SetAlias` for compatibility.
- **Confidence:** high (measured: the branch list shows the typo).

### 5.2 Per-jet scalars are stored as length-1 vectors
- **Location:** `src/JetAnalyzer.cc:8-12` (`jetPt`, `jetEta`, `jetPhi`, `jetEnergy`, `jetNConstituents`)
- **Problem:** one jet per entry, but each scalar is a `vector<double>` of size 1. That is awkward to read and adds overhead.
- **Library impact:** no. The values are correct. *Measured:* the summed constituent E equals `jetEnergy` to within 1e-12.
- **Fix:** use scalar branches. This changes the format, so coordinate with readers.
- **Confidence:** high.

### 5.3 No provenance in the output
- **Problem:** the seed, the config contents, the selection, the clustering parameters and the jetlib version are not stored.
- **Library impact:** yes, for traceability.
- **Fix:** write them to the file as `TNamed` or `TParameter` objects.
- **Confidence:** high.

---

## 6. CLI, build and documentation

| # | Location | Problem | Library impact | Fix | Confidence |
|---|---|---|---|---|---|
| 6.1 | `jetlib.cc:29-32` | `parse()` is called twice. *Measured:* "Config file specified" is printed twice. Harmless now, but any side effect in `parse` would happen twice. | no | Delete the second call. | high |
| 6.2 | `ArgumentParser.h:50` | `--help` returns `false`, so `main` exits 1. | no | Return a separate "exit 0" state. | high (measured) |
| 6.3 | `ArgumentParser.h:53` | Any unrecognised argument containing `.root` is silently taken as the output file (e.g. `foo.rootx`). | no | Accept only `-f`, or require an `.root` suffix. | high |
| 6.4 | `ArgumentParser.h:15,68` | The default config `../config/ttbar.cfg` depends on the working directory. `--help` claims the default is `config/ttbar.cfg`. Running from a job directory without `-c` aborts (2.5). | no | Make `-c` required, or resolve the path relative to the source or install directory. Fix the help text. | high |
| 6.5 | `EventGenerator.h:16` | Uses `std::ifstream` without `#include <fstream>`. It compiles only through Pythia's includes. | no | Add the include. | high |
| 6.6 | `CMakeLists.txt:29` | The FastJet path `/opt/fastjet-3.5.0` is hardcoded. The `include/` directory does not exist. ROOT is requested with only `RIO Net`, although `Tree` is used. The "Geant4" comment is stale. | no | Use a `FASTJET_DIR` cache variable, add `Tree` to the components, and remove the dead lines. | high |
| 6.7 | `CMakeLists.txt:59` | pybind11 is always required, even to build just the CLI. | no | Add an option such as `JETLIB_PYTHON=ON`. | high |
| 6.8 | `README.md` | Does not mention `git submodule update --init`, `-c`, the selection, the output branches, or the Python module. | no | Extend the README. Phase 2 adds the new options. | high |
| 6.9 | `.gitignore` | Contains only `build/*`. A local `.venv/` would show up as untracked. | no | Add `.venv/`. | high |
| 6.10 | design | The jets include underlying-event and MPI particles, with no pileup. That is a reasonable default, but undocumented. | conditional | Document it, and make MPI configurable through the config. | medium |

---

## Verified as not a problem

- Build: clean in the container, with no warnings.
- Constituent completeness: the sum of `jetConstituentsE` equals `jetEnergy` to within 1e-12, and DICE's branches `jetConstituentsPx/Py/Pz/PdgId` are present.
- With a fixed seed in the config, master is fully reproducible (identical contents across two runs).
- In 4000 ttbar events with a valid config, no `pythia.next()` failures occurred.

---

# Phase 2 results

Branch `feature/selection-seed`, not pushed. Commits:

| Commit | Change |
|---|---|
| `3791732` | **A.** Configurable jet selection: `--pt-min`, `--pt-max`, `--eta-min`, `--eta-max`, `--abs-eta`. The hardcoded filter is removed. The defaults are pT > 30 and signed η > 1.7, all cuts strict. The clustering threshold is `min(20, --pt-min)`. The selection is validated and printed. `JetSelection` is exposed in Python. |
| `a4bc542` | **B.** `--seed <int>`, default 0 (time-based). It is applied after the config file and before `pythia.init()`, and accepts the range [0, 900000000]. The seed is printed. `EventGenerator.setSeed` is exposed in Python. |
| `8eb870e` | **C.** Requested by the user after the review. Invisible particles (`!isVisible()`) are never clustered. Prompt e/μ (no hadron from hadronization or decays among their ancestors) are removed by default, and `--keep-prompt-leptons` keeps them. The mode and the numbers of removed particles are printed. `setKeepPromptLeptons` is exposed in Python. |
| `fa8ba83` | Acceptance checks and first results. |
| `fca60b9` | **D.** Requested by the user after Phase 2. The default selection becomes pT > 30 and −3 < η < 3 (|η| < 3 with `--abs-eta`), replacing the master-compatible η > 1.7. Setting one η bound keeps the default for the other. |

The review findings this resolves: 1.1 (selection), 1.2 (neutrinos), 1.3 (prompt leptons, now configurable), 1.5 (seed override; the seed is still not stored in the file), and 1.6 (clustering threshold). The other findings are still open, as agreed.

**Behaviour changes to note:**
- With C and D, the default output differs from master. C removes the fake neutrino and lepton "jets". D replaces the single forward region η > 1.7 with the central range −3 < η < 3. The "identical to master" check is therefore run against commit B. To get master's η selection back, use `--eta-min 1.7 --eta-max 100`.
- Python users of `EventGenerator` and `JetAnalysis` now also get invisibles and prompt leptons removed by default. `JetAnalyzer` without a selection still accepts all jets, as before.

## Acceptance checks

The script is `checks/check_output.py`. It runs the binaries in the DICE container and compares the tree contents with uproot. Setup: ttbar, 2000 events per run, `--seed 12345`. The master reference is unchanged master `7bc2c3b`, with the seed set in a temporary copy of the config under `$SCRATCH`. The reference builds are git worktrees in `/ceph/jkiesele/temp_data/jetlib/builds/{master,commitB}`.

```
.venv/bin/python checks/check_output.py --final-bin build/jetlib \
    --b-bin /ceph/jkiesele/temp_data/jetlib/builds/commitB/build/jetlib \
    --master-bin /ceph/jkiesele/temp_data/jetlib/builds/master/build/jetlib \
    --workdir /ceph/jkiesele/temp_data/jetlib/checks
```

**Result: 19/19 checks passed.**

| Check | Result |
|---|---|
| Defaults identical to master (commit B, same seed) | PASS: 1508 jets, all branches identical |
| Same `--seed` twice gives identical contents | PASS |
| `--seed 0` twice gives different contents (runs started more than 1 s apart) | PASS |
| `--abs-eta --eta-min 1.7 --eta-max 2.6 --pt-min 30`: all jets inside the cuts | PASS: 1706 jets, pT ≥ 30.00, \|η\| in [1.700, 2.600] |
| … both η signs balanced | PASS: 847 with η > 0, 859 with η < 0 (difference 12, limit 3√n = 124) |
| `--pt-min 10` gives jets with 10 < pT < 20 (default η range) | PASS: 3687 of 14066 jets |
| Final build, default selection pT > 30 and −3 < η < 3, both signs | PASS: 8401 jets, η in [−2.998, 3.000), 4215 with η > 0 and 4186 with η < 0 |
| No neutrinos in the output | PASS: 0 neutrino constituents, in both default and keep mode |
| Prompt leptons removed by default and kept with the flag | PASS: jets with one e/μ carrying > 50% of E: 0.62% by default, 7.45% with `--keep-prompt-leptons` (default η range). The remaining 0.62% are hard semileptonic heavy-flavour decays, which are kept by design |
| 10 invalid argument sets (negative or reversed cuts, non-numbers, missing values, seed −1, 900000001, 1.5, missing) | PASS: each exits 1 with an `Error: …` message |

The numbers above are from the rerun after D. Other validation of C, from the run logs of the earlier η > 1.7 runs: in 2000 ttbar events, 1033 prompt leptons were removed (0.52 per event). That matches the expectation from W→e/μ, directly or through τ. 205 jets still contain e/μ from hadron decays, which is intended.

Build: the container build is clean, with no warnings, for both `jetlib` and `jetlibpy`. The Python bindings were tested for `JetSelection`, `setSeed` (out-of-range values raise `ValueError`), `setKeepPromptLeptons` and `setClusterPtMin`.

## New finding from Phase 2

### 7.1 Time-based seeds have 1 s resolution
- **Location:** Pythia's handling of `Random:seed = 0`, which is the default of `--seed` and of all configs.
- **Problem:** *Measured:* two `--seed 0` runs started at the same moment produced **identical** output. Batch jobs that start in the same second therefore generate the same events, and a merged library would contain duplicate jets.
- **Library impact:** yes, for parallel production with the default seed.
- **Fix:** jetlib now prints a warning at startup and in the README. For production, give every job a distinct `--seed`. Further options: store the effective seed in the output, or derive the default seed from time plus PID or a job ID.
- **Confidence:** high (measured).

---

# Phase 3: fixing the remaining findings

## Batch 1: robustness, with the jet output unchanged

| Commit | Findings | Change |
|---|---|---|
| `a6eda6c` | 1.4, 1.7 | A config line that Pythia rejects, or a failed `init()`, throws with the line number. Failed `next()` calls are retried until the requested number of events is accepted, and the run aborts after more than `Main:timesAllowErrors` failures (configurable, default 10). The number of failures is printed. |
| `b279d1a` | 2.1, 2.4 | The output file is opened before generation, and a failed open throws. The tree is created in the analyzer's own file (`TDirectory::TContext`). `JetAnalyzer` owns its `TFile` through `unique_ptr` and is non-copyable. |
| `3456646` | 2.2, 2.5, 6.1, 6.2, 6.3 | `-n` must be a positive integer. Exceptions are caught in `main` and reported as `Error: …` with exit 1. Arguments are parsed once. `--help` exits 0. Only a positional argument ending in `.root` is taken as the output file. |

**Verification:**
- Regression: with `--seed 777` and 1000 events, the output is identical to the Phase 2 reference binary (`ref_e3e6770`) for four option sets: default, `--abs-eta --eta-min 1.7 --eta-max 2.6`, `--pt-min 10`, and `--keep-prompt-leptons`.
- `checks/check_output.py` now also takes `--ref-bin` and checks the failure modes. **Result: 27/27 checks passed**. New in this round:
  - `-n -1`, `-n abc`, `-n 0` and `foo.rootx` exit 1 with a message.
  - A config typo exits 1 and reports "Pythia did not accept line 7 … 'Top:gg2ttbarr = on'".
  - An unwritable output path fails after 2 s (with n = 2000), before generation.
  - `--help` exits 0.
- In Python, `JetAnalyzer("/nonexistent/x.root")` raises `RuntimeError`. With two analyzers, the tree lands in the first analyzer's file (115 jets), as intended.

**Still open:**
- Batch 2: 3.1 memory (streaming), 1.8 event index, 5.3 provenance.
- Batch 3, which breaks the format and needs a decision: 5.1, 5.2.
- Also open: 2.3 filter parser, 4.1–4.3 Python bindings, 6.4 default config path, 6.6–6.10 build, docs and design, and 7.1 (time seed; a warning is printed).
