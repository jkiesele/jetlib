#include "JetAnalyzer.h"
#include <sstream>
#include <cmath>
#include <iostream>
#include <algorithm>

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
    tree->Branch("jetConsituentsEta", &jetConsituentsEta_);
    tree->Branch("jetConstituentsPhi", &jetConstituentsPhi_);
    tree->Branch("jetConstituentsPdgId", &jetConstituentsPdgId_);
}

void JetAnalyzer::setFilter(const std::string& filter) {
    if (filter.empty()) {
        filterConditions_ = nullptr;
        return;
    }
    
    // Remove all spaces
    std::string cleanFilter = filter;
    cleanFilter.erase(std::remove(cleanFilter.begin(), cleanFilter.end(), ' '), cleanFilter.end());
    
    filterConditions_ = parseFilter(cleanFilter);
}

std::function<bool(const fastjet::PseudoJet&)> JetAnalyzer::parseFilter(const std::string& filter) {
    return parseOrExpression(filter);
}

std::function<bool(const fastjet::PseudoJet&)> JetAnalyzer::parseOrExpression(const std::string& expr) {
    std::vector<std::function<bool(const fastjet::PseudoJet&)>> orConditions;
    
    // Split by || at the top level (not inside parentheses)
    int parenDepth = 0;
    size_t start = 0;
    
    for (size_t i = 0; i < expr.length(); ++i) {
        if (expr[i] == '(') parenDepth++;
        else if (expr[i] == ')') parenDepth--;
        else if (parenDepth == 0 && i + 1 < expr.length() && expr.substr(i, 2) == "||") {
            orConditions.push_back(parseAndExpression(expr.substr(start, i - start)));
            start = i + 2;
            i++; // Skip second |
        }
    }
    orConditions.push_back(parseAndExpression(expr.substr(start)));
    
    if (orConditions.size() == 1) {
        return orConditions[0];
    }
    
    // Return OR of all conditions
    return [orConditions](const fastjet::PseudoJet& jet) -> bool {
        for (const auto& cond : orConditions) {
            if (cond(jet)) return true;
        }
        return false;
    };
}

std::function<bool(const fastjet::PseudoJet&)> JetAnalyzer::parseAndExpression(const std::string& expr) {
    std::vector<std::function<bool(const fastjet::PseudoJet&)>> andConditions;
    
    // Split by && at the top level (not inside parentheses)
    int parenDepth = 0;
    size_t start = 0;
    
    for (size_t i = 0; i < expr.length(); ++i) {
        if (expr[i] == '(') parenDepth++;
        else if (expr[i] == ')') parenDepth--;
        else if (parenDepth == 0 && i + 1 < expr.length() && expr.substr(i, 2) == "&&") {
            andConditions.push_back(parseCondition(expr.substr(start, i - start)));
            start = i + 2;
            i++; // Skip second &
        }
    }
    andConditions.push_back(parseCondition(expr.substr(start)));
    
    if (andConditions.size() == 1) {
        return andConditions[0];
    }
    
    // Return AND of all conditions
    return [andConditions](const fastjet::PseudoJet& jet) -> bool {
        for (const auto& cond : andConditions) {
            if (!cond(jet)) return false;
        }
        return true;
    };
}

std::function<bool(const fastjet::PseudoJet&)> JetAnalyzer::parseCondition(const std::string& expr) {
    // Handle parentheses
    if (expr.front() == '(' && expr.back() == ')') {
        return parseOrExpression(expr.substr(1, expr.length() - 2));
    }
    
    // Parse simple condition: var op value
    std::string var, op;
    double value;
    bool useAbs = false;
    
    // Check for abs()
    size_t varStart = 0;
    size_t varEnd = expr.length();
    
    if (expr.find("abs(") == 0) {
        useAbs = true;
        varStart = 4;
        size_t closeParen = expr.find(')');
        if (closeParen == std::string::npos) {
            throw std::runtime_error("Mismatched parentheses in abs()");
        }
        var = expr.substr(4, closeParen - 4);
        varEnd = closeParen + 1;
    } else {
        // Find operator position
        size_t opPos = expr.find_first_of("><=!");
        if (opPos == std::string::npos) {
            throw std::runtime_error("No operator found in condition: " + expr);
        }
        var = expr.substr(0, opPos);
        varEnd = opPos;
    }
    
    // Parse operator and value
    size_t opPos = expr.find_first_of("><=!", varEnd);
    if (opPos == std::string::npos) {
        throw std::runtime_error("No operator found in condition: " + expr);
    }
    
    if (opPos + 1 < expr.length() && expr[opPos + 1] == '=') {
        op = expr.substr(opPos, 2);
        value = std::stod(expr.substr(opPos + 2));
    } else {
        op = expr.substr(opPos, 1);
        value = std::stod(expr.substr(opPos + 1));
    }
    
    // Create lambda for this condition
    return [var, op, value, useAbs](const fastjet::PseudoJet& jet) -> bool {
        double jetValue = 0.0;
        
        if (var == "pt") jetValue = jet.pt();
        else if (var == "eta") jetValue = jet.eta();
        else if (var == "phi") jetValue = jet.phi();
        else if (var == "e" || var == "energy") jetValue = jet.e();
        else {
            std::cerr << "Warning: Unknown variable '" << var << "'" << std::endl;
            return true;
        }
        
        if (useAbs) jetValue = std::abs(jetValue);
        
        if (op == ">") return jetValue > value;
        if (op == "<") return jetValue < value;
        if (op == ">=") return jetValue >= value;
        if (op == "<=") return jetValue <= value;
        if (op == "==") return std::abs(jetValue - value) < 1e-6;
        if (op == "!=") return std::abs(jetValue - value) >= 1e-6;
        
        return true;
    };
}

bool JetAnalyzer::passesFilter(const fastjet::PseudoJet& jet) const {
    if (!selection_.pass(jet)) return false;
    if (!filterConditions_) return true;
    return filterConditions_(jet);
}

void JetAnalyzer::analyze(const JetCollection& jetCollection) {
    // Create the tree in this analyzer's file, not in whatever gDirectory currently is
    // (another JetAnalyzer may have opened a file in the meantime).
    TDirectory::TContext context(outputFile_.get());
    TTree* tree = new TTree("JetTree", "Jet Analysis Tree");
    setBranches(tree);

    for (const auto& jets : jetCollection) {
        for (const auto& jet : jets) {
            if (!passesFilter(jet)) continue;
            
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
                jetConsituentsEta_.push_back(constituent.eta());
                jetConstituentsPhi_.push_back(constituent.phi_std());
                jetConstituentsPdgId_.push_back(constituent.user_index());
            }
            tree->Fill();
            clear();
        }
    }

    outputFile_->Write();
}
