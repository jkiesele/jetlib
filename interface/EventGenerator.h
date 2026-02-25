#ifndef EventGenerator_h
#define EventGenerator_h

#include "Pythia8/Pythia.h"
#include "JetAnalysis.h"
#include "JetCollection.h"

class EventGenerator {
  public:
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
      pythia_.init();
    }
    
    void generateEvents(size_t nEvents) {
      for (size_t iEvent = 0; iEvent < nEvents; ++iEvent) {
        if (!pythia_.next()) continue;
        const Pythia8::Event& event = pythia_.event;
        auto jetAnalysis = std::make_unique<JetAnalysis>(); 
        jetAnalysis->convertPythiaToPseudoJets(event);
        jetAnalysis->clusterJets(0.4);
        auto jets = jetAnalysis->getJets(20.0);    
        jetCollection_.push_back(jets);
        jetAnalysisCollection_.push_back(std::move(jetAnalysis));
      }
    }

    const JetCollection& getJetCollection() const {
      return jetCollection_;
    }

  private:
    Pythia8::Pythia pythia_;
    JetCollection jetCollection_;
    std::vector<std::unique_ptr<JetAnalysis>> jetAnalysisCollection_;
    std::string configFile_;
};

#endif
