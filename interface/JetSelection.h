#ifndef JetSelection_h
#define JetSelection_h

#include "fastjet/PseudoJet.hh"
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

// Rectangular jet selection in pT and eta. All cuts are strict (> min, < max).
// A default-constructed selection accepts every jet.
struct JetSelection {
    static constexpr double kInf = std::numeric_limits<double>::infinity();

    double ptMin  = 0.0;
    double ptMax  = kInf;
    double etaMin = -kInf;
    double etaMax = kInf;
    bool absEta   = false;   // apply the eta cuts to |eta|

    // Throws std::invalid_argument with a readable message if the cuts are inconsistent.
    void validate() const {
        if (std::isnan(ptMin) || std::isnan(ptMax) || std::isnan(etaMin) || std::isnan(etaMax))
            throw std::invalid_argument("selection values must not be NaN");
        if (ptMin < 0) throw std::invalid_argument("pt-min must not be negative (got " + fmt(ptMin) + ")");
        if (ptMax <= 0) throw std::invalid_argument("pt-max must be positive (got " + fmt(ptMax) + ")");
        if (!(ptMin < ptMax))
            throw std::invalid_argument("pt-min (" + fmt(ptMin) + ") must be smaller than pt-max (" + fmt(ptMax) + ")");
        if (!(etaMin < etaMax))
            throw std::invalid_argument("eta-min (" + fmt(etaMin) + ") must be smaller than eta-max (" + fmt(etaMax) + ")");
        if (absEta && etaMax <= 0)
            throw std::invalid_argument("with abs-eta, eta-max must be positive (got " + fmt(etaMax) + ")");
        if (absEta && etaMin < 0 && std::isfinite(etaMin))
            throw std::invalid_argument("with abs-eta, eta-min must not be negative (got " + fmt(etaMin) + ")");
    }

    bool pass(const fastjet::PseudoJet& jet) const {
        const double pt = jet.pt();
        const double eta = absEta ? std::abs(jet.eta()) : jet.eta();
        return pt > ptMin && pt < ptMax && eta > etaMin && eta < etaMax;
    }

    std::string describe() const {
        std::ostringstream s;
        const std::string e = absEta ? "|eta|" : "eta";
        s << "pt > " << ptMin;
        if (std::isfinite(ptMax)) s << " && pt < " << ptMax;
        if (std::isfinite(etaMin)) s << " && " << e << " > " << etaMin;
        if (std::isfinite(etaMax)) s << " && " << e << " < " << etaMax;
        return s.str();
    }

  private:
    static std::string fmt(double v) { std::ostringstream s; s << v; return s.str(); }
};

#endif
