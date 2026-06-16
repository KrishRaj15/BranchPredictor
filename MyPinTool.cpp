#include "pin.H"
#include <vector>
#include <fstream>
#include <cstdint>
#include <mutex>

struct BranchInfo {
    uint64_t pc;
    uint64_t target;
    bool taken;
};
ADDRINT mainLow = 0, mainHigh = 0;

std::vector<BranchInfo> branches;
std::mutex mtx;
long long total_ins = 0;

VOID count_ins() { 
    total_ins++; 
}

void Taken(uint64_t pc, uint64_t tgt) {
    std::lock_guard<std::mutex> lock(mtx);
    branches.push_back({pc, tgt, true});
}

void NotTaken(uint64_t pc) {
    std::lock_guard<std::mutex> lock(mtx);
    branches.push_back({pc, 0ULL, false});
}

void TookBranch(uint64_t pc, uint64_t tgt) {
    Taken(pc, tgt);
}

void NotTookBranch(uint64_t pc) {
    NotTaken(pc);
}


VOID Image(IMG img, VOID *v) {
    if (IMG_IsMainExecutable(img)) {
        mainLow = IMG_LowAddress(img);
        mainHigh = IMG_HighAddress(img);
    }
}

VOID InsHandler(INS ins, VOID *v) {
    ADDRINT x = INS_Address(ins);
    if(x < mainLow || x > mainHigh) {return;}
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_ins, IARG_END);
    
    if (INS_IsBranch(ins)&&INS_HasFallThrough(ins)) {
        INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)TookBranch,
                       IARG_UINT64, INS_Address(ins),
                       IARG_BRANCH_TARGET_ADDR, IARG_END);
        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)NotTookBranch,
                       IARG_UINT64, INS_Address(ins), IARG_END);
    }
}


VOID Fini(INT32 code, VOID *v) {
    std::ofstream out("new_trace.txt");
    
    // Write a text header for readability
    out << "BRANCH_TRACE_FILE" << "\n";
    out << "Total_Instructions: " << total_ins << "\n";
    out << "Total_Branches: " << branches.size() << "\n";
    out << "-----------------------------------------------\n";
    out << "PC                 Target             Taken\n";
    out << "-----------------------------------------------\n";

    // Iterate and write each branch entry as text
    for (const auto& b : branches) {
        out << "0x" << std::hex << b.pc << " "
            << "0x" << std::hex << b.target << " "
            << std::dec << (b.taken ? 1 : 0) << "\n";
    }

    out.close();
    fprintf(stderr, "Wrote %lu branches (text), %lld total ins\n", (unsigned long)branches.size(), total_ins);
}
int main(int argc, char *argv[]) {
    PIN_Init(argc, argv);
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(InsHandler, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}

