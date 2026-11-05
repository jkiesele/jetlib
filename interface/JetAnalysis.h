#ifndef JetAnalysis_h
#define JetAnalysis_h

#include "fastjet/ClusterSequence.hh"
#include "fastjet/PseudoJet.hh"

#include "Pythia8/Pythia.h"
#include <vector>
#include <memory>

class JetAnalysis {
  public:
    JetAnalysis() {};
    ~JetAnalysis() {};

    // ptMin: minimum jet pT kept by the clustering step
    void clusterJets(double R, double ptMin = 20.0);
    std::vector<fastjet::PseudoJet> getJets(double ptMin) const {
      std::vector<fastjet::PseudoJet> selectedJets;
      for (const auto& jet : jets_) {
        if (jet.pt() >= ptMin) {
          selectedJets.push_back(jet);
        }
      }
      return selectedJets;
    }

    // Converts the visible final-state particles of a Pythia event to PseudoJets.
    // Invisible particles (neutrinos, BSM invisibles) are always dropped.
    // Prompt leptons (e/mu without a hadron among their ancestors, e.g. from W, Z
    // or tau decays) are dropped unless keepPromptLeptons is true.
    void convertPythiaToPseudoJets(const Pythia8::Event& event, bool keepPromptLeptons = false);

    // True for a final-state e/mu with no hadron (from hadronization or decays) in its
    // mother1 chain. Beam particles are not counted as hadron ancestors.
    static bool isPromptLepton(const Pythia8::Event& event, int i);

    size_t nRemovedInvisible() const { return nRemovedInvisible_; }
    size_t nRemovedPromptLeptons() const { return nRemovedPromptLeptons_; }

    void clear() {
      jets_.clear();
      particles_.clear();
    }

  private:
    std::vector<fastjet::PseudoJet> jets_, particles_;
    std::unique_ptr<fastjet::ClusterSequence> clusterSeq_;
    size_t nRemovedInvisible_ = 0;
    size_t nRemovedPromptLeptons_ = 0;
};

#endif