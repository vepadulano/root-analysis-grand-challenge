#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <tuple>

#include <RtypesCore.h>
#include <ROOT/RDataFrame.hxx>
#include <ROOT/RDFHelpers.hxx> // VariationsFor
#include <ROOT/RResultPtr.hxx> // CloneResultAndAction
#include <ROOT/RSnapshotOptions.hxx>
#include <ROOT/RDF/RDatasetSpec.hxx>
#include <ROOT/RDF/RInterface.hxx> // ChangeEmptyEntryRange, ChangeSpec
#include <ROOT/RDF/RResultMap.hxx> // CloneResultAndAction
#include <TSystem.h>               // AccessPathName

using ROOT::Internal::RDF::ChangeEmptyEntryRange;
using ROOT::Internal::RDF::ChangeSpec;
using ROOT::Internal::RDF::CloneResultAndAction;

ROOT::RVecI def_jet_pt_mask(const ROOT::RVecF &jet_pt)
{
    // std::cout << "jet_pt_mask: " << std::endl;
    // std::cout << "jet_pt: " << std::endl;
    // for (const auto &el : jet_pt)
    //     std::cout << el << ", ";
    // std::cout << std::endl;
    return jet_pt > 25;
}

bool njets_ge_4(const ROOT::RVecI &jet_pt_mask)
{
    std::cout << "njets_ge_4: " << std::endl;
    std::cout << "jet_pt_mask: " << std::endl;
    for (const auto &el : jet_pt_mask)
        std::cout << el << ", ";
    std::cout << std::endl;

    bool res = ROOT::VecOps::Sum(jet_pt_mask) >= 4;
    std::cout << "Result: " << res << std::endl;
    return res;
}

ROOT::RVecD vary_2(const ROOT::RVecF &jet_pt, const ROOT::RVecI &jet_pt_mask)
{
    std::cout << "vary_2: " << std::endl;
    // std::cout << "jet_pt: " << std::endl;
    // for (const auto &el : jet_pt)
    //     std::cout << el << ", ";
    // std::cout << std::endl;
    auto test = ROOT::VecOps::Take(jet_pt[jet_pt_mask], 4);
    return ROOT::RVecD{-1, 1};
}

static ROOT::RVecF jet_pt_0_nominal{165.298, 160.236, 66.8572, 40.8195, 36.1471, 34.4265, 16.1777, 10.8605, 10.8463};
static ROOT::RVec<ROOT::RVecF> jet_pt_0_variations{
    {170.257, 165.043, 68.8629, 42.0441, 37.2315, 35.4593, 16.663, 11.1863, 11.1717},
    {186.183, 168.403, 68.9531, 39.7356, 35.7335, 34.0872, 16.2116, 11.4339, 9.48373}};

static ROOT::RVecF jet_pt_1_nominal{42.7032, 40.627, 28.4609, 24.7615, 22.6275, 13.3896, 12.236};
static ROOT::RVec<ROOT::RVecF> jet_pt_1_variations{
    {43.9843, 41.8458, 29.3147, 25.5043, 23.3063, 13.7912, 12.6031},
    {42.5948, 37.4309, 27.6115, 25.3204, 21.9652, 13.2168, 11.485}};

static ROOT::RVec<ROOT::RVecF> jet_pt_nominals{jet_pt_0_nominal, jet_pt_1_nominal};
static ROOT::RVec<ROOT::RVec<ROOT::RVecF>> jet_pt_variations{jet_pt_0_variations, jet_pt_1_variations};

std::tuple<ROOT::RDF::RResultPtr<TH1D>, ROOT::RDF::Experimental::RResultMap<TH1D>> analysis(ROOT::RDF::RNode &df)
{

    auto df1 = df.Define("jet_pt", [](ULong64_t entry)
                         { return jet_pt_nominals[entry]; },
                         {"rdfentry_"});

    auto df2 = df1.Vary("jet_pt", [](ULong64_t entry)
                        { return jet_pt_variations[entry]; },
                        {"rdfentry_"}, {"pt_scale_up", "pt_res_up"});

    auto df3 = df2.Define("jet_pt_mask", def_jet_pt_mask, {"jet_pt"}).Filter(njets_ge_4, {"jet_pt_mask"});

    auto df4 = df3.Vary("weights",
                        vary_2,
                        {"jet_pt", "jet_pt_mask"},
                        2);

    auto h = df4.Histo1D<ROOT::RVecF, double>({"NAME", "TITLE", 10, -10, 10}, "jet_pt", "weights");

    auto vars = ROOT::RDF::Experimental::VariationsFor(h);

    return {h, vars};
}

int main()
{
    ROOT::RDataFrame d{2};
    auto df = d.Define("weights", []() -> double
                       { return 0; });
    ROOT::RDF::RNode df_rnode = ROOT::RDF::AsRNode(df);
    ChangeEmptyEntryRange(df_rnode, {1, 2});

    auto [h, vars] = analysis(df_rnode);
    ROOT::Internal::RDF::TriggerRun(df_rnode);

    auto h_clone = ROOT::Internal::RDF::CloneResultAndAction(h);
    auto vars_clone = ROOT::Internal::RDF::CloneResultAndAction(vars);
    ChangeEmptyEntryRange(df_rnode, {1, 2});

    ROOT::Internal::RDF::TriggerRun(df_rnode);
}