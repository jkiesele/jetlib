#ifndef ArgumentParser_h
#define ArgumentParser_h

#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include "JetSelection.h"

class ArgumentParser {
  private:
    std::string filename_;
    std::string configFile_;
    bool debug_;
    long nEvents_;
    bool helpRequested_ = false;
    JetSelection selection_;
    long seed_;
    bool keepPromptLeptons_;

    // Parse a floating-point option value; the whole string must be a finite number.
    static bool parseDouble(const std::string& opt, const char* text, double& out) {
        std::string s(text);
        try {
            size_t pos = 0;
            out = std::stod(s, &pos);
            if (pos == s.size() && std::isfinite(out)) return true;
        } catch (const std::exception&) {}
        std::cerr << "Error: " << opt << " expects a number, got '" << s << "'" << std::endl;
        return false;
    }

    bool etaMinSet_ = false;

    // Default selection: pt > 30 GeV and -3 < eta < 3 (|eta| < 3 with --abs-eta).
    static constexpr double kDefaultPtMin = 30.0;
    static constexpr double kDefaultEtaMax = 3.0;
    static JetSelection defaultSelection() {
        JetSelection sel;
        sel.ptMin = kDefaultPtMin;
        sel.etaMin = -kDefaultEtaMax;
        sel.etaMax = kDefaultEtaMax;
        return sel;
    }
    
  public:
    ArgumentParser() : filename_("output.root"), configFile_("../config/ttbar.cfg"), debug_(false), nEvents_(1), selection_(defaultSelection()), seed_(0), keepPromptLeptons_(false) {}
    
    bool parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "--nEvents" || arg == "-n") {
                if (i + 1 < argc) {
                    std::string s(argv[++i]);
                    long long v = 0;
                    size_t pos = 0;
                    bool ok = false;
                    try { v = std::stoll(s, &pos); ok = (pos == s.size()); } catch (const std::exception&) {}
                    if (!ok || v < 1) {
                        std::cerr << "Error: " << arg << " expects a positive integer, got '" << s << "'" << std::endl;
                        return false;
                    }
                    nEvents_ = static_cast<long>(v);
                } else {
                    std::cerr << "Error: " << arg << " requires a number argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--config" || arg == "-c") {
                if (i + 1 < argc) {
                    configFile_ = argv[++i];
                    std::cout << "Config file specified: " << configFile_ << std::endl;
                } else {
                    std::cerr << "Error: " << arg << " requires a filename argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--pt-min" || arg == "--pt-max" || arg == "--eta-min" || arg == "--eta-max") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: " << arg << " requires a number argument" << std::endl;
                    return false;
                }
                double v;
                if (!parseDouble(arg, argv[++i], v)) return false;
                if (arg == "--pt-min") selection_.ptMin = v;
                else if (arg == "--pt-max") selection_.ptMax = v;
                else if (arg == "--eta-min") { selection_.etaMin = v; etaMinSet_ = true; }
                else selection_.etaMax = v;
            }
            else if (arg == "--seed") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --seed requires an integer argument" << std::endl;
                    return false;
                }
                std::string s(argv[++i]);
                long long v = 0;
                size_t pos = 0;
                bool ok = false;
                try { v = std::stoll(s, &pos); ok = (pos == s.size()); } catch (const std::exception&) {}
                if (!ok) {
                    std::cerr << "Error: --seed expects an integer, got '" << s << "'" << std::endl;
                    return false;
                }
                if (v < 0 || v > 900000000) {
                    std::cerr << "Error: --seed must be in [0, 900000000], got " << s
                              << ". Pythia's maximum is 900000000; negative seeds (Pythia's default seed)"
                              << " are not accepted by jetlib. Use 0 to let jetlib draw a seed." << std::endl;
                    return false;
                }
                seed_ = static_cast<long>(v);
            }
            else if (arg == "--keep-prompt-leptons") {
                keepPromptLeptons_ = true;
            }
            else if (arg == "--abs-eta") {
                selection_.absEta = true;
            }
            else if (arg == "--debug" || arg == "-d") {
                debug_ = true;
            }
            else if (arg == "--filename" || arg == "-f") {
                if (i + 1 < argc) {
                    filename_ = argv[++i];
                } else {
                    std::cerr << "Error: " << arg << " requires a filename argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                helpRequested_ = true;
                return false;
            }
            else if (arg[0] != '-' && arg.size() > 5 && arg.compare(arg.size() - 5, 5, ".root") == 0) {
                // a positional argument ending in .root is the output filename
                filename_ = arg;
            }
            else {
                std::cerr << "Error: unknown argument: " << arg << std::endl;
                printUsage(argv[0]);
                return false;
            }
        }
        // With --abs-eta the default lower bound -3 is meaningless: the default becomes |eta| < 3.
        if (selection_.absEta && !etaMinSet_) selection_.etaMin = -JetSelection::kInf;
        try {
            selection_.validate();
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: invalid jet selection: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    
    void printUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -c, --config <file>      Configuration file for Pythia (default: config/ttbar.cfg)" << std::endl;
        std::cout << "  -f, --filename <file>    Output ROOT filename (default: output.root)" << std::endl;
        std::cout << "  -n, --nEvents <number>   Number of events to generate, >= 1 (default: 1)" << std::endl;
        std::cout << "  -d, --debug              Enable debug mode" << std::endl;
        std::cout << "      --seed <int>         Pythia random seed in [0, 900000000], overrides the config file" << std::endl;
        std::cout << "                           (default: 0 = jetlib draws a random seed; it is printed and" << std::endl;
        std::cout << "                           stored in the output, so the run can be reproduced)" << std::endl;
        std::cout << "      --keep-prompt-leptons  Cluster prompt e/mu (from W/Z/tau, not from hadron decays)" << std::endl;
        std::cout << "                           (default: removed before clustering)" << std::endl;
        std::cout << "  Neutrinos and other invisible particles are never clustered." << std::endl;
        std::cout << std::endl;
        std::cout << "Jet selection (all cuts strict; default: pt > 30 && -3 < eta < 3):" << std::endl;
        std::cout << "      --pt-min <GeV>       Minimum jet pT (default: 30, must be >= 0)" << std::endl;
        std::cout << "      --pt-max <GeV>       Maximum jet pT (default: none)" << std::endl;
        std::cout << "      --eta-min <value>    Minimum jet eta (default: -3; none with --abs-eta)" << std::endl;
        std::cout << "      --eta-max <value>    Maximum jet eta (default: 3)" << std::endl;
        std::cout << "      --abs-eta            Apply the eta cuts to |eta| (default: signed eta)" << std::endl;
        std::cout << "  The clustering threshold is min(20 GeV, pt-min), so it never cuts harder than --pt-min." << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << program_name << " -c myconfig.cfg -f myoutput.root -n 100" << std::endl;
  
    }
    
    const std::string& getConfigFile() const { return configFile_; }
    const std::string& getFilename() const { return filename_; }
    bool isDebug() const { return debug_; }
    long getNEvents() const { return nEvents_; }
    // true if parse() returned false because --help was given (not an error)
    bool helpRequested() const { return helpRequested_; }
    const JetSelection& getSelection() const { return selection_; }
    long getSeed() const { return seed_; }
    bool keepPromptLeptons() const { return keepPromptLeptons_; }
};

#endif
