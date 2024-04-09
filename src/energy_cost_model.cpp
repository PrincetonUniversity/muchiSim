#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <map>
using namespace std;
#include "configs/param_energy.h"
#include "configs/param_cost.h"
#include "common/macros.h"

map<string, double> vars;
#include "common/calc_energy.h"
#include "common/calc_perf.h"
#include "common/calc_cost.h"

std::vector<std::string> keys = {
    "total_noc_messages",
    "total_inter_board_traffic",
    "total_inter_die_traffic",
    "total_inter_pack_traffic",
    "total_ruche_traffic",
    "all_loaded_params",
    "total_flops",
    "total_ops",
    "total_loads",
    "total_stores",
    "pcache_hits",
    "pcache_misses",
    "dcache_hits",
    "dcache_misses",
    "total_mc_transactions",
    "dram_active_words",
    "nanoseconds",
    "TOT_SYSTEM_AREA",
    "DIE_SIDE_MM"
};

void calculation(){
    const u_int64_t total_noc_messages = vars["total_noc_messages"];
    const u_int64_t total_inter_board_traffic = vars["total_inter_board_traffic"];
    const u_int64_t total_inter_die_traffic = vars["total_inter_die_traffic"];
    const u_int64_t total_inter_pack_traffic = vars["total_inter_pack_traffic"];
    const u_int64_t total_ruche_traffic = vars["total_ruche_traffic"];
    const u_int64_t all_loaded_params = vars["all_loaded_params"];

    const u_int64_t total_flops = vars["total_flops"];
    const u_int64_t total_ops = vars["total_ops"];
    const u_int64_t total_loads = vars["total_loads"];
    const u_int64_t total_stores = vars["total_stores"];

    const u_int64_t pcache_hits = vars["pcache_hits"];
    const u_int64_t pcache_misses = vars["pcache_misses"];
    const u_int64_t dcache_hits = vars["dcache_hits"];
    const u_int64_t dcache_misses = vars["dcache_misses"];
    const u_int64_t total_mc_transactions = vars["total_mc_transactions"];
    const u_int64_t dram_active_words = vars["dram_active_words"];
    const double nanoseconds = vars["nanoseconds"];
    const double sim_time = vars["sim_time"];

    cout << "\n=== ENERGY RESULTS ===\n";
    print_energy(total_noc_messages, total_inter_board_traffic, total_inter_die_traffic, total_inter_pack_traffic, total_ruche_traffic, total_ops, total_flops, total_loads, total_stores, all_loaded_params, pcache_hits, pcache_misses, dcache_hits, dcache_misses, total_mc_transactions, dram_active_words, nanoseconds, cout);

    cout << "\n=== PERF RESULTS ===\n";
    print_perf(true, total_mc_transactions, total_noc_messages, total_inter_board_traffic, total_inter_die_traffic, total_inter_pack_traffic, total_ruche_traffic, total_ops, total_flops, total_loads, total_stores, sim_time, nanoseconds);

    cout << "\n=== COST RESULTS ===\n";
    cost_calculation();
}


int main(int argc, char** argv) {
    string filename = "counters.txt";
    if (argc >= 2) filename = argv[1];
    ifstream inputFile(filename);
    string line;
    cout.imbue(locale(""));
    if (inputFile.is_open()) {
        while (getline(inputFile, line)) {
            string counterName;
            string s;

            size_t colonPosition = line.find(":");
            if (colonPosition != string::npos) {
                counterName = line.substr(0, colonPosition);
                s = line.substr(colonPosition + 1);

                // Convert string to double
                // cout << "Counter name: " << counterName << ", Value: " << s << endl;
                s.erase(std::remove(s.begin(), s.end(), ','), s.end());
                double counterValue = stod(s);

                // Store counter in area
                vars[counterName] = counterValue;

                // Display
                cout << "Counter name: " << counterName << ", Value: " << vars[counterName] << endl;
            } else {
                cout << "Invalid line format: " << line << endl;
            }
        }
        inputFile.close();

        // Check that all the keys are found in the counters file
        for (const auto& key : keys) {
            auto it = vars.find(key);
            if (it == vars.end()) {
                std::cerr << "ERROR: '" << key << "' not found in area map." << std::endl;
                return 1;
            }
        }
        calculation();

    } else {
        cout << "Unable to open file." << endl;
    }
    cout << endl;
    return 0;
}