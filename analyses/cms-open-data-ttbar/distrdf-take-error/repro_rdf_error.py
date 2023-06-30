import ROOT

NPARTITIONS = 2
NCORES = 1
TREENAME = "events"
# xrdcp root://eoscms.cern.ch//eos/cms/store/test/agc/datasets/merged/TT_TuneCUETP8M1_13TeV-powheg-pythia8/1.root repro_file.root
FILENAME = "repro_file.root"


def init_functions():
    helper_path = "helper.cpp"
    ROOT.gSystem.CompileMacro(helper_path, "kO")


def analysis():

    d = ROOT.RDataFrame(TREENAME, FILENAME)
    # FUNCTION PARAMETERS
    num_bins = 25
    bin_low = 50
    bin_high = 550
    process = "ttbar"
    variation = "nominal"

    # normalization for MC
    x_sec = 729.84
    nevts_total = 442122
    lumi = 3378  # /pb
    xsec_weight = x_sec * lumi / nevts_total


    d = d.Define("weights", str(xsec_weight))  # default weights

    # jet_pt variations definition
    # pt_scale_up() and pt_res_up(jet_pt) return scaling factors applying to jet_pt
    # pt_scale_up() - jet energy scaly systematic
    # pt_res_up(jet_pt) - jet resolution systematic

    d = d.Vary("jet_pt",
                "ROOT::RVec<ROOT::RVecF>{jet_pt*pt_scale_up(), jet_pt*jet_pt_resolution(jet_pt.size())}",
                ["pt_scale_up", "pt_res_up"])

    # event selection - the core part of the algorithm applied for both regions
    # selecting events containing at least one lepton and four jets with pT > 25 GeV
    # applying requirement at least one of them must be b-tagged jet (see details in the specification)
    d = d.Define("jet_pt_mask", "jet_pt>25").Filter("Sum(jet_pt_mask) >= 4")

    # b-tagging variations for nominal samples
    d = d.Vary("weights",
                "ROOT::RVecD{weights*btag_weight_variation(jet_pt[jet_pt_mask])}",
                [f"{weight_name}_{direction}" for weight_name in [f"btag_var_{i}" for i in range(4)] for direction in [
                    "up", "down"]]
                ) if variation == "nominal" else d


    region, observable = ("4j1b", "HT")


    # only one b-tagged region required
    # observable is total transvesre momentum
    fork = d.Filter("Sum(jet_btag[jet_pt_mask]>=0.5)==1").Define(observable, "Sum(jet_pt[jet_pt_mask])")


    # fill histogram for observable column in RDF object
    res = fork.Histo1D((f"{process}_{variation}_{region}", process, num_bins,
                        bin_low, bin_high), observable, "weights")

    vars = ROOT.RDF.Experimental.VariationsFor(res)

    res.GetValue()

def main():
    init_functions()
    analysis()


if __name__ == "__main__":
    raise SystemExit(main())
