#include "JetAnalyzer.h"
#include "TNamed.h"
#include <sstream>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

void JetAnalyzer::setBranches(TTree* tree) {
    tree->Branch("jetPt", &jetPt_);
    tree->Branch("jetEta", &jetEta_);
    tree->Branch("jetPhi", &jetPhi_);
    tree->Branch("jetEnergy", &jetEnergy_);
    tree->Branch("jetNConstituents", &jetNConstituents_);
    tree->Branch("jetConstituentsPx", &jetConstituentsPx_);
    tree->Branch("jetConstituentsPy", &jetConstituentsPy_);
    tree->Branch("jetConstituentsPz", &jetConstituentsPz_);
    tree->Branch("jetConstituentsPt", &jetConstituentsPt_);
    tree->Branch("jetConstituentsE", &jetConstituentsE_);
    tree->Branch("jetConstituentsEta", &jetConstituentsEta_);
    tree->Branch("jetConstituentsPhi", &jetConstituentsPhi_);
    tree->Branch("jetConstituentsPdgId", &jetConstituentsPdgId_);
    // event bookkeeping (plain scalars)
    tree->Branch("eventIndex", &eventIndex_, "eventIndex/L");
    tree->Branch("jetIndex", &jetIndex_, "jetIndex/I");
    if (tauJets_) {
        // generator tau matched to the jet (tau mode only)
        tree->Branch("jetTauPdgId", &tauPdgId_, "jetTauPdgId/I");
        tree->Branch("jetTauDecayMode", &tauDecayMode_, "jetTauDecayMode/I");
        tree->Branch("jetTauNProngs", &tauNProngs_, "jetTauNProngs/I");
        tree->Branch("jetTauNPi0", &tauNPi0_, "jetTauNPi0/I");
        tree->Branch("jetTauDR", &tauDR_, "jetTauDR/D");
        tree->Branch("jetTauVisPt", &tauVisPt_, "jetTauVisPt/D");
        tree->Branch("jetTauVisEta", &tauVisEta_, "jetTauVisEta/D");
        tree->Branch("jetTauVisPhi", &tauVisPhi_, "jetTauVisPhi/D");
        tree->Branch("jetTauVisE", &tauVisE_, "jetTauVisE/D");
        tree->Branch("jetTauPt", &tauPt_, "jetTauPt/D");
        tree->Branch("jetTauEta", &tauEta_, "jetTauEta/D");
        tree->Branch("jetTauPhi", &tauPhi_, "jetTauPhi/D");
        tree->Branch("jetTauE", &tauE_, "jetTauE/D");
    }
}

namespace {
// Recursive-descent parser for jet filters such as "pt > 30 && (abs(eta) < 2.5 || e >= 100)".
//   or   := and ( "||" and )*
//   and  := atom ( "&&" atom )*
//   atom := "(" or ")" | cond
//   cond := var op number      var := name | "abs(" name ")"
//   name := pt | eta | phi | e | energy   op := > < >= <= == !=   (phi in [-pi, pi))
// Every error (unknown variable or operator, missing term, trailing characters,
// unbalanced parentheses, bad number) throws std::invalid_argument when the filter is set.
using Pred = std::function<bool(const fastjet::PseudoJet&)>;

class FilterParser {
  public:
    explicit FilterParser(const std::string& s) : s_(s) {}
    Pred parse() {
        Pred p = parseOr();
        skipSpace();
        if (pos_ != s_.size()) fail("unexpected '" + s_.substr(pos_) + "'");
        return p;
    }

  private:
    const std::string& s_;
    size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& what) const {
        throw std::invalid_argument("invalid jet filter '" + s_ + "' at position " +
                                    std::to_string(pos_) + ": " + what);
    }
    void skipSpace() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
    bool accept(const std::string& tok) {
        skipSpace();
        if (s_.compare(pos_, tok.size(), tok) == 0) { pos_ += tok.size(); return true; }
        return false;
    }
    Pred parseOr() {
        std::vector<Pred> terms{parseAnd()};
        while (accept("||")) terms.push_back(parseAnd());
        if (terms.size() == 1) return terms[0];
        return [terms](const fastjet::PseudoJet& j) {
            for (const auto& t : terms) if (t(j)) return true;
            return false;
        };
    }
    Pred parseAnd() {
        std::vector<Pred> terms{parseAtom()};
        while (accept("&&")) terms.push_back(parseAtom());
        if (terms.size() == 1) return terms[0];
        return [terms](const fastjet::PseudoJet& j) {
            for (const auto& t : terms) if (!t(j)) return false;
            return true;
        };
    }
    Pred parseAtom() {
        skipSpace();
        if (pos_ >= s_.size()) fail("expected a condition, found the end of the filter");
        if (accept("(")) {
            Pred p = parseOr();
            if (!accept(")")) fail("expected ')'");
            return p;
        }
        return parseCondition();
    }
    std::string parseName() {
        skipSpace();
        const size_t start = pos_;
        while (pos_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_')) ++pos_;
        if (pos_ == start) fail("expected a variable name");
        return s_.substr(start, pos_ - start);
    }
    Pred parseCondition() {
        std::string name = parseName();
        bool useAbs = false;
        if (name == "abs") {
            if (!accept("(")) fail("expected '(' after abs");
            name = parseName();
            if (!accept(")")) fail("expected ')' after abs(" + name);
            useAbs = true;
        }
        double (*var)(const fastjet::PseudoJet&) = nullptr;
        if (name == "pt") var = [](const fastjet::PseudoJet& j) { return j.pt(); };
        else if (name == "eta") var = [](const fastjet::PseudoJet& j) { return j.eta(); };
        else if (name == "phi") var = [](const fastjet::PseudoJet& j) { return j.phi_std(); };  // [-pi, pi), as in the tree
        else if (name == "e" || name == "energy") var = [](const fastjet::PseudoJet& j) { return j.e(); };
        else fail("unknown variable '" + name + "' (known: pt, eta, phi, e, energy)");

        enum class Op { GT, LT, GE, LE, EQ, NE } op;
        if (accept(">=")) op = Op::GE;
        else if (accept("<=")) op = Op::LE;
        else if (accept("==")) op = Op::EQ;
        else if (accept("!=")) op = Op::NE;
        else if (accept(">")) op = Op::GT;
        else if (accept("<")) op = Op::LT;
        else fail("expected one of > < >= <= == != after '" + name + "'");

        skipSpace();
        const char* begin = s_.c_str() + pos_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin || !std::isfinite(value)) fail("expected a number");
        pos_ += static_cast<size_t>(end - begin);

        return [var, useAbs, op, value](const fastjet::PseudoJet& j) {
            double x = var(j);
            if (useAbs) x = std::abs(x);
            switch (op) {
                case Op::GT: return x > value;
                case Op::LT: return x < value;
                case Op::GE: return x >= value;
                case Op::LE: return x <= value;
                case Op::EQ: return std::abs(x - value) < 1e-6;
                case Op::NE: return std::abs(x - value) >= 1e-6;
            }
            return false;
        };
    }
};
}  // namespace

void JetAnalyzer::setFilter(const std::string& filter) {
    // Parse completely before replacing the current filter: an invalid filter throws
    // and leaves the analyzer unchanged. An empty (or blank) filter removes it.
    if (filter.find_first_not_of(" \t\n") == std::string::npos) {
        filterConditions_ = nullptr;
        return;
    }
    filterConditions_ = FilterParser(filter).parse();
}

bool JetAnalyzer::passesFilter(const fastjet::PseudoJet& jet) const {
    if (!selection_.pass(jet)) return false;
    if (!filterConditions_) return true;
    return filterConditions_(jet);
}

void JetAnalyzer::begin() {
    // Create the tree in this analyzer's file, not in whatever gDirectory currently is
    // (another JetAnalyzer may have opened a file in the meantime). Because the tree
    // belongs to a file, ROOT flushes its baskets to disk while filling.
    TDirectory::TContext context(outputFile_.get());
    tree_ = new TTree("JetTree", "Jet Analysis Tree");
    setBranches(tree_);
}

void JetAnalyzer::fill(const std::vector<fastjet::PseudoJet>& jets, long long eventIndex) {
    fill(jets, eventIndex, nullptr);
}

void JetAnalyzer::fill(const std::vector<fastjet::PseudoJet>& jets, long long eventIndex,
                       const std::vector<TauTruth>* taus) {
    if (!tree_) throw std::logic_error("JetAnalyzer::fill called before begin()");
    if (tauJets_ && !taus) throw std::logic_error("JetAnalyzer::fill: tau mode needs the generator taus");
    // Selected jets keep their clustering order in the tree; jetIndex is their pT rank
    // among the selected jets of this event (0 = hardest).
    std::vector<size_t> selected;
    // tau mode: index of the matched tau per jet (-1 = none) and the deltaR
    std::vector<int> tauIdx(jets.size(), -1);
    std::vector<double> tauDR(jets.size(), -1);
    for (size_t i = 0; i < jets.size(); ++i) {
        if (!passesFilter(jets[i])) continue;
        if (tauJets_) {
            tauIdx[i] = matchTau(jets[i], *taus, kTauMatchDR, tauDR[i]);
            if (tauIdx[i] < 0) continue;
        }
        selected.push_back(i);
    }
    if (tauJets_) {
        // one jet per tau: if a tau's decay products form several selected jets, keep the closest
        std::vector<size_t> unique;
        for (size_t i : selected) {
            bool closest = true;
            for (size_t j : selected)
                if (j != i && tauIdx[j] == tauIdx[i] && (tauDR[j] < tauDR[i] || (tauDR[j] == tauDR[i] && j < i)))
                    closest = false;
            if (closest) unique.push_back(i);
        }
        selected.swap(unique);
    }
    std::vector<size_t> byPt(selected);
    std::stable_sort(byPt.begin(), byPt.end(),
                     [&](size_t a, size_t b) { return jets[a].pt() > jets[b].pt(); });
    std::vector<int> rank(jets.size(), -1);
    for (size_t r = 0; r < byPt.size(); ++r) rank[byPt[r]] = static_cast<int>(r);

    // jet.constituents() needs the jets' ClusterSequence to be alive during this call
    for (size_t i : selected) {
        const auto& jet = jets[i];
        eventIndex_ = eventIndex;
        jetIndex_ = rank[i];
        if (tauJets_) {
            const TauTruth& t = (*taus)[tauIdx[i]];
            tauPdgId_ = t.pdgId; tauDecayMode_ = t.decayMode; tauNProngs_ = t.nProngs; tauNPi0_ = t.nPi0;
            tauDR_ = tauDR[i];
            tauVisPt_ = t.visP4.pt(); tauVisEta_ = t.visP4.eta(); tauVisPhi_ = t.visP4.phi_std(); tauVisE_ = t.visP4.e();
            tauPt_ = t.p4.pt(); tauEta_ = t.p4.eta(); tauPhi_ = t.p4.phi_std(); tauE_ = t.p4.e();
        }

        jetPt_.push_back(jet.pt());
        jetEta_.push_back(jet.eta());
        jetPhi_.push_back(jet.phi_std());
        jetEnergy_.push_back(jet.e());
        jetNConstituents_.push_back(jet.constituents().size());

        for (const auto& constituent : jet.constituents()) {
            jetConstituentsPx_.push_back(constituent.px());
            jetConstituentsPy_.push_back(constituent.py());
            jetConstituentsPz_.push_back(constituent.pz());
            jetConstituentsE_.push_back(constituent.e());
            jetConstituentsPt_.push_back(constituent.pt());
            jetConstituentsEta_.push_back(constituent.eta());
            jetConstituentsPhi_.push_back(constituent.phi_std());
            jetConstituentsPdgId_.push_back(constituent.user_index());
        }
        tree_->Fill();
        clear();
    }
}

void JetAnalyzer::finish() {
    if (!tree_) throw std::logic_error("JetAnalyzer::finish called before begin()");
    metadata_["n_jets_written"] = std::to_string(tree_->GetEntries());
    TDirectory* dir = outputFile_->mkdir("jetlib", "jetlib provenance", true);
    for (const auto& kv : metadata_) {
        TNamed entry(kv.first.c_str(), kv.second.c_str());
        dir->WriteTObject(&entry, kv.first.c_str(), "Overwrite");
    }
    outputFile_->Write();
    tree_ = nullptr;
}

void JetAnalyzer::analyze(const JetCollection& jetCollection) {
    begin();
    for (size_t i = 0; i < jetCollection.size(); ++i) fill(jetCollection[i], static_cast<long long>(i));
    finish();
}
