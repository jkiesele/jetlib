#ifndef EventGenerator_h
#define EventGenerator_h

#include "Pythia8/Pythia.h"
#include "JetAnalysis.h"
#include "JetCollection.h"
#include <fstream>
#include <functional>
#include <stdexcept>

class EventGenerator {
  public:
    static constexpr long kSeedMin = 0;           // 0 = time-based seed in Pythia
    static constexpr long kSeedMax = 900000000;   // Pythia's upper limit for Random:seed

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
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        if (!pythia_.readString(line))
          throw std::runtime_error("Pythia did not accept line " + std::to_string(lineNumber) +
                                   " of " + configFile_ + ": '" + line + "'");
      }
      infile.close();
      // A seed set via setSeed() overrides whatever the config file says.
      if (hasSeed_) {
        if (!pythia_.readString("Random:setSeed = on") ||
            !pythia_.readString("Random:seed = " + std::to_string(seed_)))
          throw std::runtime_error("Pythia did not accept the random seed " + std::to_string(seed_));
      }
      if (!pythia_.init())
        throw std::runtime_error("Pythia initialization failed for " + configFile_ +
                                 " (see the PYTHIA messages above)");
    }
    
    // Called once per accepted event with its clustered jets and the event index
    // (0-based, counting accepted events). The jets' ClusterSequence is alive only
    // during the call, so constituents must be used or copied inside it.
    using EventCallback = std::function<void(const std::vector<fastjet::PseudoJet>& jets, size_t eventIndex)>;

    // Generates events until nEvents have been accepted. Failed pythia.next() calls are
    // counted and retried; more failures than Main:timesAllowErrors (config, default 10)
    // abort the run with an exception.
    // With onEvent, each event is handed over and then freed (constant memory).
    // Without it, all jets and their ClusterSequences are kept for getJetCollection().
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
        jetAnalysis->clusterJets(0.4, clusterPtMin_);
        auto jets = jetAnalysis->getJets(clusterPtMin_);
        if (onEvent) {
          onEvent(jets, eventIndex);   // jetAnalysis (and its ClusterSequence) is freed afterwards
        } else {
          jetCollection_.push_back(jets);
          jetAnalysisCollection_.push_back(std::move(jetAnalysis));
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
    // number of failed (and retried) pythia.next() calls
    size_t getNFailedEvents() const { return nFailedEvents_; }
    // number of accepted events over all generateEvents() calls
    size_t getNAcceptedEvents() const { return nAcceptedEvents_; }

    // Override the random seed of the config file. 0 means time-based seeding.
    // Negative values (Pythia's "default seed") are rejected by jetlib policy.
    // Must be called before initialize().
    void setSeed(long seed) {
      if (seed < kSeedMin || seed > kSeedMax)
        throw std::invalid_argument("seed must be in [0, 900000000], got " + std::to_string(seed) +
          " (Pythia's maximum is 900000000; negative seeds, which Pythia would map to its"
          " default seed, are not accepted by jetlib)");
      seed_ = seed;
      hasSeed_ = true;
    }

    const JetCollection& getJetCollection() const {
      return jetCollection_;
    }

  private:
    Pythia8::Pythia pythia_;
    JetCollection jetCollection_;
    std::vector<std::unique_ptr<JetAnalysis>> jetAnalysisCollection_;
    std::string configFile_;
    double clusterPtMin_ = 20.0;
    bool hasSeed_ = false;
    bool keepPromptLeptons_ = false;
    size_t nRemovedInvisible_ = 0;
    size_t nRemovedPromptLeptons_ = 0;
    size_t nFailedEvents_ = 0;
    size_t nAcceptedEvents_ = 0;
    long seed_ = 0;
};

#endif
