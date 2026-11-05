#ifndef EventGenerator_h
#define EventGenerator_h

#include "Pythia8/Pythia.h"
#include "JetAnalysis.h"
#include "JetCollection.h"
#include <fstream>
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
      while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        pythia_.readString(line);
      }
      infile.close();
      // A seed set via setSeed() overrides whatever the config file says.
      if (hasSeed_) {
        pythia_.readString("Random:setSeed = on");
        pythia_.readString("Random:seed = " + std::to_string(seed_));
      }
      pythia_.init();
    }
    
    void generateEvents(size_t nEvents) {
      for (size_t iEvent = 0; iEvent < nEvents; ++iEvent) {
        if (!pythia_.next()) continue;
        const Pythia8::Event& event = pythia_.event;
        auto jetAnalysis = std::make_unique<JetAnalysis>(); 
        jetAnalysis->convertPythiaToPseudoJets(event);
        jetAnalysis->clusterJets(0.4, clusterPtMin_);
        auto jets = jetAnalysis->getJets(clusterPtMin_);    
        jetCollection_.push_back(jets);
        jetAnalysisCollection_.push_back(std::move(jetAnalysis));
      }
    }

    // Minimum jet pT kept after clustering (default 20 GeV). Must not exceed the
    // pT threshold of the jet selection applied later.
    void setClusterPtMin(double ptMin) {
      if (!(ptMin >= 0)) throw std::invalid_argument("cluster pT threshold must not be negative");
      clusterPtMin_ = ptMin;
    }
    double getClusterPtMin() const { return clusterPtMin_; }

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
    long seed_ = 0;
};

#endif
