#ifndef EventGenerator_h
#define EventGenerator_h

#include "Pythia8/Pythia.h"
#include "JetAnalysis.h"
#include "JetCollection.h"
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <chrono>
#include <cstdint>
#include <random>
#include <unistd.h>
#include <stdexcept>

class EventGenerator {
  public:
    static constexpr long kSeedMin = 0;           // 0 = jetlib draws a random seed
    static constexpr long kSeedMax = 900000000;   // Pythia's upper limit for Random:seed
    static constexpr double kJetR = 0.4;          // anti-kT radius

    EventGenerator(const std::string& configFile) : configFile_(configFile) {};
    ~EventGenerator() {};

    // intitialize with a configuration file
    // the function parsers line by line and applies the settings to the Pythia instance
    void initialize() {
      std::ifstream infile(configFile_);
      if (!infile.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + configFile_);
      }
      std::string line;
      int lineNumber = 0;
      while (std::getline(infile, line)) {
        ++lineNumber;
        configText_ += line + "\n";
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        if (!pythia_.readString(line))
          throw std::runtime_error("Pythia did not accept line " + std::to_string(lineNumber) +
                                   " of " + configFile_ + ": '" + line + "'");
      }
      infile.close();
      // Seed: a value from setSeed() overrides the config file. A requested seed of 0
      // (from setSeed or from the config) is replaced by a seed drawn here, so that the
      // seed actually used is known and jobs started at the same time differ.
      if (hasSeed_) requestedSeed_ = seed_;
      else if (pythia_.flag("Random:setSeed")) requestedSeed_ = pythia_.mode("Random:seed");
      else requestedSeed_ = -1;   // config leaves seeding off: Pythia's default seed
      if (requestedSeed_ >= 0) {
        effectiveSeed_ = (requestedSeed_ == 0) ? drawSeed() : requestedSeed_;
        if (!pythia_.readString("Random:setSeed = on") ||
            !pythia_.readString("Random:seed = " + std::to_string(effectiveSeed_)))
          throw std::runtime_error("Pythia did not accept the random seed " + std::to_string(effectiveSeed_));
      } else {
        effectiveSeed_ = -1;
      }
      if (!pythia_.init())
        throw std::runtime_error("Pythia initialization failed for " + configFile_ +
                                 " (see the PYTHIA messages above)");
    }
    
    // Called once per accepted event with its clustered jets and the event index
    // (0-based, counting accepted events). The jets keep their ClusterSequence alive;
    // it is freed after the call unless the callback keeps copies of the jets.
    using EventCallback = std::function<void(const std::vector<fastjet::PseudoJet>& jets, size_t eventIndex)>;

    // Generates events until nEvents have been accepted. Failed pythia.next() calls are
    // counted and retried; more failures than Main:timesAllowErrors (config, default 10)
    // abort the run with an exception.
    // With onEvent, each event is handed over and then freed (constant memory).
    // Without it, all jets (and with them their ClusterSequences) are kept for getJetCollection().
    void generateEvents(size_t nEvents, const EventCallback& onEvent = nullptr) {
      const int maxFailed = pythia_.mode("Main:timesAllowErrors");
      for (size_t accepted = 0; accepted < nEvents; ) {
        if (!pythia_.next()) {
          if (++nFailedEvents_ > static_cast<size_t>(maxFailed))
            throw std::runtime_error("pythia.next() failed " + std::to_string(nFailedEvents_) +
                                     " times, more than Main:timesAllowErrors = " + std::to_string(maxFailed));
          continue;
        }
        ++accepted;
        const size_t eventIndex = nAcceptedEvents_++;
        const Pythia8::Event& event = pythia_.event;
        auto jetAnalysis = std::make_unique<JetAnalysis>(); 
        jetAnalysis->convertPythiaToPseudoJets(event, keepPromptLeptons_);
        nRemovedInvisible_ += jetAnalysis->nRemovedInvisible();
        nRemovedPromptLeptons_ += jetAnalysis->nRemovedPromptLeptons();
        for (const auto& [id, n] : jetAnalysis->removedUndecayed()) {
          removedUndecayed_[id] += n;
          nRemovedUndecayed_ += n;
        }
        jetAnalysis->clusterJets(kJetR, clusterPtMin_);
        auto jets = jetAnalysis->getJets(clusterPtMin_);
        if (onEvent) {
          onEvent(jets, eventIndex);
        } else {
          jetCollection_.push_back(std::move(jets));
        }
      }
    }

    // Minimum jet pT kept after clustering (default 20 GeV). Must not exceed the
    // pT threshold of the jet selection applied later.
    void setClusterPtMin(double ptMin) {
      if (!(ptMin >= 0)) throw std::invalid_argument("cluster pT threshold must not be negative");
      clusterPtMin_ = ptMin;
    }
    double getClusterPtMin() const { return clusterPtMin_; }

    // Prompt leptons (e/mu not from hadron decays) are removed before clustering
    // unless this is set to true. Invisible particles are always removed.
    void setKeepPromptLeptons(bool keep) { keepPromptLeptons_ = keep; }
    bool getKeepPromptLeptons() const { return keepPromptLeptons_; }
    size_t getNRemovedInvisible() const { return nRemovedInvisible_; }
    size_t getNRemovedPromptLeptons() const { return nRemovedPromptLeptons_; }
    // final-state particles whose Pythia decay failed (always removed), total and per PDG ID
    size_t getNRemovedUndecayed() const { return nRemovedUndecayed_; }
    const std::map<int, size_t>& getRemovedUndecayed() const { return removedUndecayed_; }
    // e.g. "1 (10221: 1)", or "0"
    std::string describeRemovedUndecayed() const {
      std::string s = std::to_string(nRemovedUndecayed_);
      if (removedUndecayed_.empty()) return s;
      s += " (";
      bool first = true;
      for (const auto& [id, n] : removedUndecayed_) {
        s += (first ? "" : ", ") + std::to_string(id) + ": " + std::to_string(n);
        first = false;
      }
      return s + ")";
    }
    // number of failed (and retried) pythia.next() calls
    size_t getNFailedEvents() const { return nFailedEvents_; }
    // number of accepted events over all generateEvents() calls
    size_t getNAcceptedEvents() const { return nAcceptedEvents_; }
    // full text of the config file read by initialize()
    const std::string& getConfigText() const { return configText_; }
    double getPythiaVersion() { return pythia_.parm("Pythia:versionNumber"); }

    // Override the random seed of the config file. 0 means jetlib draws a seed in
    // initialize() (see getSeed()). Negative values (Pythia's "default seed") are
    // rejected by jetlib policy. Must be called before initialize().
    void setSeed(long seed) {
      if (seed < kSeedMin || seed > kSeedMax)
        throw std::invalid_argument("seed must be in [0, 900000000], got " + std::to_string(seed) +
          " (Pythia's maximum is 900000000; negative seeds, which Pythia would map to its"
          " default seed, are not accepted by jetlib)");
      seed_ = seed;
      hasSeed_ = true;
    }

    // Seed actually used, valid after initialize(): the requested seed, or the drawn one
    // if 0 was requested; -1 if the config switched seeding off (Pythia default seed).
    long getSeed() const { return effectiveSeed_; }
    // seed requested via setSeed() or the config (0 = draw one); -1 if seeding is off
    long getRequestedSeed() const { return requestedSeed_; }

    // A seed in [1, 900000000] from std::random_device, mixed with the time in ns and
    // the process ID, so that jobs started simultaneously get different seeds.
    static long drawSeed() {
      std::random_device rd;
      uint64_t x = (static_cast<uint64_t>(rd()) << 32) ^ rd();
      x ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
      x ^= static_cast<uint64_t>(::getpid()) << 17;
      // splitmix64 finalizer to spread the bits
      x += 0x9e3779b97f4a7c15ULL;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
      x ^= x >> 31;
      return static_cast<long>(x % static_cast<uint64_t>(kSeedMax)) + 1;
    }

    const JetCollection& getJetCollection() const {
      return jetCollection_;
    }

  private:
    Pythia8::Pythia pythia_;
    JetCollection jetCollection_;
    std::string configFile_;
    std::string configText_;
    double clusterPtMin_ = 20.0;
    bool hasSeed_ = false;
    bool keepPromptLeptons_ = false;
    size_t nRemovedInvisible_ = 0;
    size_t nRemovedPromptLeptons_ = 0;
    size_t nRemovedUndecayed_ = 0;
    std::map<int, size_t> removedUndecayed_;
    size_t nFailedEvents_ = 0;
    size_t nAcceptedEvents_ = 0;
    long seed_ = 0;
    long requestedSeed_ = 0;
    long effectiveSeed_ = 0;
};

#endif
