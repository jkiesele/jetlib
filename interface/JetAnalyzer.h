#ifndef JetAnalyzer_h
#define JetAnalyzer_h

#include <vector>
#include "TFile.h"
#include "JetCollection.h"
#include "JetSelection.h"
#include "TTree.h"
#include <string>
#include <functional>
class JetAnalyzer {
  public:
    JetAnalyzer(std::string outputFilename = "jet_analysis.root") {
      outputFile_ = TFile::Open(outputFilename.c_str(), "RECREATE");
    }
    ~JetAnalyzer() {
      outputFile_->Close();
    }

    void analyze(const JetCollection& jetCollection);
    void setFilter(const std::string& filter);
    // Cut-based selection, applied in addition to an optional string filter.
    void setSelection(const JetSelection& selection) { selection.validate(); selection_ = selection; }
    const JetSelection& getSelection() const { return selection_; }
  private:
    TFile* outputFile_;

    std::vector<double> jetPt_;
    std::vector<double> jetEta_;
    std::vector<double> jetPhi_;
    std::vector<double> jetEnergy_;
    std::vector<int> jetNConstituents_;
    std::vector<double> jetConstituentsPx_;
    std::vector<double> jetConstituentsPy_;
    std::vector<double> jetConstituentsPz_;
    std::vector<double> jetConstituentsPt_;
    std::vector<double> jetConstituentsE_;
    std::vector<double> jetConsituentsEta_;
    std::vector<double> jetConstituentsPhi_;
    std::vector<int> jetConstituentsPdgId_;

    void setBranches(TTree* tree);

    void clear() {
      jetPt_.clear();
      jetEta_.clear();
      jetPhi_.clear();
      jetEnergy_.clear();
      jetNConstituents_.clear();
      jetConstituentsPx_.clear();
      jetConstituentsPy_.clear();
      jetConstituentsPz_.clear();
      jetConstituentsPt_.clear();
      jetConstituentsE_.clear();
      jetConsituentsEta_.clear();
      jetConstituentsPhi_.clear();
      jetConstituentsPdgId_.clear();
    }

    std::function<bool(const fastjet::PseudoJet&)> parseFilter(const std::string& filter);
    std::function<bool(const fastjet::PseudoJet&)> parseOrExpression(const std::string& expr);
    std::function<bool(const fastjet::PseudoJet&)> parseAndExpression(const std::string& expr);
    std::function<bool(const fastjet::PseudoJet&)> parseCondition(const std::string& expr);
    bool passesFilter(const fastjet::PseudoJet& jet) const;

    //std::vector<std::function<bool(const fastjet::PseudoJet&)>> filterConditions_;
    std::function<bool(const fastjet::PseudoJet&)> filterConditions_;
    JetSelection selection_;
};
#endif