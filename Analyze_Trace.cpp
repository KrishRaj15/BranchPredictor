#include <bits/stdc++.h>
#include <sstream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "json.hpp"
using namespace std;
struct BranchInfo {
    uint64_t pc;
    uint64_t target;
    bool taken;
};

struct PredictorResult {
    double accuracy;
    unordered_map<uint64_t, uint8_t> final_table;
};

using json = nlohmann::json;


void print_json_results(const vector<BranchInfo>& trace, double acc_at, double acc_ant, 
                        double acc1b, double acc2b, double acc_loop, double acc_hybrid, 
                        double acc_racbp, const unordered_map<uint64_t, uint8_t>& btb) {
    json j;
    
    // Accuracies for all predictors
    j["metrics"] = {
        {"Total Branches", (double)trace.size()},
        {"Always Taken", acc_at},
        {"Always Not Taken", acc_ant},
        {"1-Bit Predictor", acc1b},
        {"2-Bit Predictor", acc2b},
        {"Loop Predictor", acc_loop},
        {"GShare Predictor", acc_hybrid},
        {"RACBP", acc_racbp}
    };

    j["btb_total_entries"] = btb.size();

    // Export address states for the primary predictor (2-bit/Hybrid)
    for (const auto& entry : btb) {
        std::stringstream ss;
        ss << "0x" << std::hex << entry.first;
        j["btb_state"].push_back({
            {"address", ss.str()},
            {"state", entry.second}
        });
    }

    std::cout << j.dump(4) << std::endl;
}

double always_taken_accuracy(const vector<BranchInfo>& trace) {
    int correct = 0;
    for (const auto& b : trace) {
        if (b.taken) ++correct;
    }
    return 100.0 * correct / trace.size();
}

double always_nottaken_accuracy(const vector<BranchInfo>& trace) {
    int correct = 0;
    for (const auto& b : trace) {
        if (!b.taken) ++correct;
    }
    return 100.0 * correct / trace.size();
}

double one_bit_accuracy(const vector<BranchInfo>& trace) {
    unordered_map<uint64_t, bool> table;
    int correct = 0;
    for (const auto& b : trace) {
        uint64_t idx = b.pc & 0xFFF;
        bool& state = table[idx];
        bool pred = state;
        if (pred == b.taken) ++correct;
        state = b.taken;
    }
    return 100.0 * correct / trace.size();
}

PredictorResult two_bit_accuracy(const vector<BranchInfo>& trace) {
    unordered_map<uint64_t, uint8_t> table;
    int correct = 0;
    for (const auto& b : trace) {
        uint64_t idx = b.pc & 0xFFF;
        if(table.find(idx)==table.end()){
            table[idx]=2;
        }
        uint8_t&s = table[idx];
        bool pred = (s >= 2);
        if (pred == b.taken) ++correct;
        if (b.taken) {
            if (s < 3) ++s;
        } else {
            if (s > 0) --s;
        }
    }
    return {100.0 * correct / trace.size(),table};
}
int local=0,gshare=0;

double gshare_accuracy(const vector<BranchInfo>& trace) {
    const int HISTORY_BITS = 12;
    const int SIZE = 1 << HISTORY_BITS;

    vector<uint8_t> gshare_table(SIZE, 2);  

    uint32_t GHR = 0; // Global History Register
    int correct = 0;

    for (const auto& b : trace) {
        
        // Gshare index: XOR the PC with the GHR
        uint32_t gshare_idx = (b.pc ^ GHR) & (SIZE - 1);
        
        // Get the current 2-bit state for this index
        uint8_t& gshare_state = gshare_table[gshare_idx];
        
        bool gshare_pred = (gshare_state >= 2);

        if (gshare_pred == b.taken) correct++;

        if (b.taken) {
            if (gshare_state < 3) gshare_state++;
        } else {
            if (gshare_state > 0) gshare_state--;
        }

        GHR = ((GHR << 1) | b.taken) & (SIZE - 1);
    }

    return 100.0 * correct / trace.size();
}

struct LoopEntry {
    uint32_t count = 0;
    uint32_t target_count = 0;
    bool conf = false;
    bool loop_direction = true;
    bool initialized = false;
};

double loop_predictor_accuracy(const vector<BranchInfo>& trace) {
    unordered_map<uint64_t, LoopEntry> table;
    int correct = 0;

    for (const auto& b : trace) {
        LoopEntry& entry = table[b.pc];

        if (!entry.initialized) {
            entry.loop_direction = b.taken;
            entry.initialized = true;
        }

        // Predict
        bool pred = entry.loop_direction; 
        if (entry.conf && entry.count == entry.target_count) {
            pred = !entry.loop_direction; 
        }

        if (pred == b.taken) ++correct;

        if (b.taken == entry.loop_direction) {
            entry.count++;
        } else {
            // We hit an exit (or a direction change)
            if (entry.count == entry.target_count && entry.count > 0) {
                entry.conf = true;
            } else {
                entry.target_count = entry.count;
                entry.conf = false;
            }
            entry.count = 0;
            //wrong direction
            if (!entry.conf && entry.target_count == 0) {
                entry.loop_direction = b.taken;
            }
        }
    }
    return 100.0 * correct / trace.size();
}

// RACBP State Entry
struct RACBPEntry {
    int8_t conf = 0;   // Signed confidence: -8 to +7
    int8_t regret = 0; // Regret magnitude: 0 to MAX_REGRET
};

double racbp_accuracy(const vector<BranchInfo>& trace) {
    const int HISTORY_BITS = 12;
    const int SIZE = 1 << HISTORY_BITS;
    
    vector<RACBPEntry> table(SIZE);
    uint32_t GHR = 0;
    int correct = 0;

    // Tuned hardware parameters
    const int8_t MAX_CONF = 7;           
    const int8_t MIN_CONF = -8;          
    const int8_t MAX_REGRET = 7;         
    const int8_t HIGH_CONF_THRESH = 4;   

    for (const auto& b : trace) {
        uint32_t idx = (b.pc ^ GHR) & (SIZE - 1);
        RACBPEntry& entry = table[idx];

        
        int sign = (entry.conf >= 0) ? 1 : -1;
        
        int decision_value = entry.conf - (sign * entry.regret);

        //0 results in last value
        bool pred;
        if (decision_value == 0) {
            pred = (entry.conf >= 0); 
        } else {
            pred = (decision_value > 0);
        }

        if (pred == b.taken) ++correct;

        //Update Logic
        bool is_high_conf = abs(entry.conf) >= HIGH_CONF_THRESH;

        if (pred == b.taken) {
            if (entry.regret > 0) entry.regret--;
            
            if (b.taken && entry.conf < MAX_CONF) entry.conf++;
            else if (!b.taken && entry.conf > MIN_CONF) entry.conf--;
            
        } else {
            // Misprediction
            if (is_high_conf) {
                entry.regret = min((int)MAX_REGRET, entry.regret + 2); 
                
                if (entry.conf > 0) entry.conf = 0; // Weakly Taken/Neutral
                else entry.conf = -1;               // Weakly Not Taken
            } else {
                entry.regret = min((int)MAX_REGRET, entry.regret + 1);
                
                if (b.taken && entry.conf < MAX_CONF) entry.conf++;
                else if (!b.taken && entry.conf > MIN_CONF) entry.conf--;
            }
        }

        // Update Global History Register (shift left, insert actual outcome)
        GHR = ((GHR << 1) | b.taken) & (SIZE - 1);
    }

    return 100.0 * correct / trace.size();
}

int main(int argc, char* argv[]) {
    // Open as a standard text file
    // 1. Check if the argument was provided
    if (argc < 2) {
        std::cerr << "Error: No trace file provided." << std::endl;
        return 1;
    }

    // 2. Use argv[1] instead of a hardcoded string
    std::string trace_file_path = argv[1]; 
    std::ifstream file(trace_file_path);
    //  ifstream file("branch_trace.txt");
    // 3. Handle file open errors so you don't fail silently
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << trace_file_path << std::endl;
        return 1; // Return non-zero so Streamlit catches the error
    }

    string line;
    uint64_t total_ins = 0;
    uint64_t branch_count = 0;

    // 1. Parse the header lines to extract metadata
    while (getline(file, line)) {
        if (line.find("Total_Instructions:") != string::npos) {
            total_ins = stoull(line.substr(line.find(":") + 1));
        } else if (line.find("Total_Branches:") != string::npos) {
            branch_count = stoull(line.substr(line.find(":") + 1));
        } else if (line.find("---") != string::npos) {
            // Found the separator, the next line is the column header "PC Target Taken"
            getline(file, line); // consume column header
            getline(file, line); // consume second separator line
            break;
        }
    }

    // 2. Read the branch data entries
    vector<BranchInfo> trace;
    trace.reserve(branch_count);

    uint64_t pc, target;
    int taken_val;
    
    // Read formatted hex (0x...) and the integer boolean
    while (file >> hex >> pc >> target >> dec >> taken_val) {
        trace.push_back({pc, target, (bool)taken_val});
    }

    file.close();

    // Verify we got the data
    if (trace.empty()) {
        cerr << "Error: No trace data loaded. Check file format.\n";
        return 1;
    }

    // 3. Perform Analysis (rest of your original logic)
    int taken_cnt = 0;
    for (const auto& b : trace) {
        if (b.taken) ++taken_cnt;
    }

    double taken_pct = 100.0 * taken_cnt / trace.size();
    
    cerr << "\nBranch Pattern Analysis (From Text Trace):\n";
    cerr << fixed << setprecision(2);
    cerr << "Total ins: " << total_ins << '\n';
    cerr << "Branches: " << trace.size() << "\n";
    cerr << "Taken: " << taken_cnt << " (" << taken_pct << "%) " 
              << "Non-taken: " << (trace.size() - taken_cnt) << '\n';

    // Calculate accuracies
    double acc_at = always_taken_accuracy(trace);
    double acc_ant = always_nottaken_accuracy(trace);
    double acc1b = one_bit_accuracy(trace);
    PredictorResult res2b = two_bit_accuracy(trace);
    double GS = gshare_accuracy(trace);
    double lp2b =loop_predictor_accuracy(trace);
    double acc_racbp = racbp_accuracy(trace);

    cerr << "\nPredictor Accuracies:\n";
    cerr << "Always Taken: " << acc_at << "%\n";
    cerr << "Always NT:    " << acc_ant << "%\n";
    cerr << "1-bit:        " << acc1b << "%\n";
    cerr << "loop :        " << lp2b  << "%\n";
    cerr << "GShare2-bit:  " << GS <<endl;
    cerr << "RACBP:        " << acc_racbp << "%\n";

    print_json_results(trace,acc_at,acc_ant,acc1b,res2b.accuracy,lp2b,GS,acc_racbp,res2b.final_table);

    return 0;
}

