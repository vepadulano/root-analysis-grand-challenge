#include <string>
#include <vector>
#include <iostream>
#include <tuple>

#include <TH1D.h>
#include <TRandom3.h>

#include <ROOT/RVec.hxx>
#include <ROOT/RDataFrame.hxx>
#include <ROOT/RDFHelpers.hxx>
#include <ROOT/RDF/RDatasetSpec.hxx>
#include <ROOT/RDF/InterfaceUtils.hxx>
#include <ROOT/RResultPtr.hxx>     // CloneResultAndAction
#include <ROOT/RDF/RInterface.hxx> // ChangeEmptyEntryRange, ChangeSpec
#include <ROOT/RDF/RResultMap.hxx> // CloneResultAndAction
#include <ROOT/RLogger.hxx>

auto verbosity =
    ROOT::Experimental::RLogScopedVerbosity(ROOT::Detail::RDF::RDFLogChannel(), ROOT::Experimental::ELogLevel::kDebug);

// functions creating systematic variations
inline TRandom &get_thread_local_trandom()
{
   thread_local TRandom rng;
   rng.SetSeed(gRandom->Integer(1000));
   return rng;
}

ROOT::RVecF jet_pt_resolution(std::size_t size)
{
   // normal distribution with 5% variations, shape matches jets
   ROOT::RVecF res(size);
   std::generate(std::begin(res), std::end(res), []()
                 { return get_thread_local_trandom().Gaus(1, 0.05); });
   return res;
}

ROOT::RVecF btag_weight_variation(const ROOT::RVecF &jet_pt)
{
   // weight variation depending on i-th jet pT (7.5% as default value, multiplied by i-th jet pT / 50 GeV)
   ROOT::RVecF res;
   for (const float &pt : ROOT::VecOps::Take(jet_pt, 4))
   {
      res.push_back(1 + .075 * pt / 50);
      res.push_back(1 - .075 * pt / 50);
   }
   return res;
}

ROOT::RVec<ROOT::RVecF> analysis_fn_01(const ROOT::RVecF &jet_pt)
{
   float pt_scale_up = 1.03f;
   auto res = ROOT::RVec<ROOT::RVecF>{jet_pt * pt_scale_up, jet_pt * jet_pt_resolution(jet_pt.size())};
   std::cout << __PRETTY_FUNCTION__ << std::endl;
   std::cout << "jet_pt: " << std::endl;
   for (const auto &el: jet_pt) std::cout << el << ", ";
   std::cout << std::endl;

   std::cout << "res: " << std::endl;
   std::cout << "res[0]: " << std::endl;
   for (const auto &el: res[0]) std::cout << el << ", ";
   std::cout << std::endl;
   std::cout << "res[1]: " << std::endl;
   for (const auto &el: res[1]) std::cout << el << ", ";
   std::cout << std::endl;

   return res;
}

ROOT::RVecI analysis_fn_02(const ROOT::RVecF &jet_pt)
{
   return jet_pt > 25;
}

bool analysis_fn_03(const ROOT::RVecI &jet_pt_mask)
{
   return ROOT::VecOps::Sum(jet_pt_mask) >= 4;
}

ROOT::RVecD analysis_fn_04(double weights, const ROOT::RVecF &jet_pt, const ROOT::RVecI &jet_pt_mask)
{
   return ROOT::RVecD{weights * btag_weight_variation(jet_pt[jet_pt_mask])};
}

bool analysis_fn_05(const ROOT::RVecD &jet_btag, const ROOT::RVecI &jet_pt_mask)
{
   return ROOT::VecOps::Sum(jet_btag[jet_pt_mask] >= 0.5) == 1;
}

float analysis_fn_06(const ROOT::RVecF &jet_pt, const ROOT::RVecI &jet_pt_mask)
{
   return ROOT::VecOps::Sum(jet_pt[jet_pt_mask]);
}

std::tuple<ROOT::RDF::RResultPtr<TH1D>, ROOT::RDF::Experimental::RResultMap<TH1D>> analysis(ROOT::RDF::RNode &df)
{
   // Function Parameters
   int num_bins = 25;
   double bin_low = 50;
   double bin_high = 550;
   auto process = "ttbar";
   auto variation = "nominal";
   auto region = "4j1b";
   auto observable = "HT";

   // normalization for MC
   double x_sec = 729.84;
   double nevts_total = 442122;
   double lumi = 3378; // pb
   double xsec_weight = x_sec * lumi / nevts_total;

   // Default weights
   auto d1 = df.Define("weights", [&xsec_weight]()
                       { return xsec_weight; });

   // jet_pt variations definition
   //  pt_scale_up() and pt_res_up(jet_pt) return scaling factors applying to jet_pt
   //  pt_scale_up() - jet energy scaly systematic
   //  pt_res_up(jet_pt) - jet resolution systematic
   auto d2 = d1.Vary("jet_pt", analysis_fn_01, {"jet_pt"}, {"pt_scale_up", "pt_res_up"});

   // event selection - the core part of the algorithm applied for both regions
   // selecting events containing at least one lepton and four jets with pT > 25 GeV
   // applying requirement at least one of them must be b-tagged jet (see details in the specification)
   auto d3 = d2.Define("jet_pt_mask", analysis_fn_02, {"jet_pt"}).Filter(analysis_fn_03, {"jet_pt_mask"});

   // b-tagging variations for nominal samples
   auto d4 = d3.Vary("weights",
                     analysis_fn_04,
                     {"weights", "jet_pt", "jet_pt_mask"},
                     {"btag_var_0_up", "btag_var_0_down", "btag_var_1_up", "btag_var_1_down", "btag_var_2_up",
                      "btag_var_2_down", "btag_var_3_up", "btag_var_3_down"});

   // only one b-tagged region required
   // observable is total transvesre momentum
   auto d5 = d4.Filter(analysis_fn_05, {"jet_btag", "jet_pt_mask"}).Define(observable, analysis_fn_06, {"jet_pt", "jet_pt_mask"});

   auto h = d5.Histo1D<float, double>({"NAME", process, num_bins, bin_low, bin_high}, observable, "weights");

   auto vars = ROOT::RDF::Experimental::VariationsFor(h);

   return {h, vars};
}

void repro()
{
   const std::string treename{"events"};
   const std::string filename{"repro_cloning.root"};

   std::vector<ROOT::RDF::Experimental::RDatasetSpec> specs;
   std::vector<std::pair<std::int64_t, std::int64_t>> ranges{{0, 1}, {1, 2}};
   specs.reserve(ranges.size());
   for (const auto &r : ranges)
   {
      ROOT::RDF::Experimental::RDatasetSpec spec;
      // Every spec represents a different portion of the global dataset
      spec.AddSample({"", treename, filename});
      spec.WithGlobalRange({r.first, r.second});
      specs.push_back(spec);
   }

   ROOT::RDataFrame df{specs[0]};
   ROOT::RDF::RNode df_rnode = ROOT::RDF::AsRNode(df);

   auto [h, vars] = analysis(df_rnode);
   ROOT::Internal::RDF::TriggerRun(df_rnode);

   auto h_clone = ROOT::Internal::RDF::CloneResultAndAction(h);
   auto vars_clone = ROOT::Internal::RDF::CloneResultAndAction(vars);
   ROOT::Internal::RDF::ChangeSpec(df_rnode, std::move(specs[1]));

   ROOT::Internal::RDF::TriggerRun(df_rnode);
}

int main()
{
   repro();
}
