#ifndef JetAnalyzer_h
#define JetAnalyzer_h

#include <vector>
#include "TFile.h"
#include "JetCollection.h"
#include "JetSelection.h"
#include "TTree.h"
#include "TDirectory.h"
#include <string>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
class JetAnalyzer {
  public:
    // Opens (recreates) the output file immediately; throws if it cannot be opened,
    // so a bad path is detected before any events are generated.
    JetAnalyzer(std::string outputFilename = "jet_analysis.root")
      : outputFile_(TFile::Open(outputFilename.c_str(), "RECREATE")) {
      if (!outputFile_ || outputFile_->IsZombie())
        throw std::runtime_error("Could not open output file: " + outputFilename);
    }
    ~JetAnalyzer() {
      if (outputFile_) outputFile_->Close();
    }
    // owns the output file: not copyable
    JetAnalyzer(const JetAnalyzer&) = delete;
    JetAnalyzer& operator=(const JetAnalyzer&) = delete;

    // Streaming interface: begin() creates the tree, fill() writes the selected jets of
    // one event, finish() writes the file. analyze() does all three for a full collection.
    void begin();
    void fill(const std::vector<fastjet::PseudoJet>& jets, long long eventIndex);
    void finish();
    // Provenance: stored by finish() as TNamed objects (name, title = value) in the
    // directory "jetlib" of the output file, together with n_jets_written.
    void setMetadata(const std::string& name, const std::string& value) { metadata_[name] = value; }
    void analyze(const JetCollection& jetCollection);
    void setFilter(const std::string& filter);
    // Cut-based selection, applied in addition to an optional string filter.
    void setSelection(const JetSelection& selection) { selection.validate(); selection_ = selection; }
    const JetSelection& getSelection() const { return selection_; }
  private:
    std::unique_ptr<TFile> outputFile_;
    TTree* tree_ = nullptr;   // owned by outputFile_
    std::map<std::string, std::string> metadata_;

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
    Long64_t eventIndex_ = 0;   // index of the accepted event in this run
    Int_t jetIndex_ = 0;        // pT rank of the jet among the selected jets of its event

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