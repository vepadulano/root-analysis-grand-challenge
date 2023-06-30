import ROOT

TREENAME = "events"
# xrdcp root://eoscms.cern.ch//eos/cms/store/test/agc/datasets/merged/TT_TuneCUETP8M1_13TeV-powheg-pythia8/1.root repro_file.root
FILENAME = "repro_file.root"
OUTFILENAME = "repro_cloning.root"

def analysis():
    opts = ROOT.RDF.RSnapshotOptions()

    df = ROOT.RDataFrame(TREENAME, FILENAME)

    # df = df.Filter("rdfentry_ < 200")
    # df = df.Filter("rdfentry_ >= 150 && rdfentry_ < 250")

    df = df.Filter("rdfentry_ == 236 || rdfentry_ == 237")
    # df = df.Filter("rdfentry_ < 2")
    opts.fAutoFlush = 1
    df.Snapshot(TREENAME, OUTFILENAME, df.GetColumnNames(), opts)

def main():
    analysis()


if __name__ == "__main__":
    raise SystemExit(main())
