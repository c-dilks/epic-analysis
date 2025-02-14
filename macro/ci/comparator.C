// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2023 Christopher Dilks

#define EXCLUDE_DELPHES

R__LOAD_LIBRARY(EpicAnalysis)

// make grids of plots, comparing data from the infiles
// - depending on infile, different histograms will be drawn
void comparator(
    TString title0="Delphes", TString infile0="out/resolution.fastsim.root",
    TString title1="ePIC",    TString infile1="out/resolution.epic.root",
    TString title2="ATHENA",  TString infile2="out/resolution.athena.root",
    TString title3="ECCE",    TString infile3="out/resolution.ecce.root",
    TString outfile="out/resolution.comparison.root",
    TString gx="x", TString gy="q2" // plotgrid vars
    ) {

  // histograms ==================================================
  // - set histogram lists, based on infile name
  std::vector<TString> histList;
  if(outfile.Contains("coverage")) {
    histList.push_back("x");
    histList.push_back("y");
    histList.push_back("W");
    histList.push_back("pLab");
    histList.push_back("pTlab");
    histList.push_back("etaLab");
    histList.push_back("phiLab");
    histList.push_back("z");
    histList.push_back("pT");
    histList.push_back("qT");
    histList.push_back("qTq");
    histList.push_back("mX");
    histList.push_back("phiH");
    histList.push_back("phiS");
    histList.push_back("phiSivers");
    histList.push_back("phiCollins");
  }
  else if(outfile.Contains("resolution")) {
    histList.push_back("x_Res");
    histList.push_back("y_Res");
    histList.push_back("pT_Res");
    histList.push_back("Q2_Res");
    histList.push_back("Nu_Res");
    histList.push_back("W_Res");
    histList.push_back("phiH_Res");
    histList.push_back("phiS_Res");
    histList.push_back("z_Res");
    histList.push_back("mX_Res");
    histList.push_back("xF_Res");
  } else {
    fprintf(stderr,"ERROR: histList not defined for specified infile name\n");
    return;
  };


  // setup =======================================================

  // plot grid variables, titles, and settings
  TString gxT, gyT;
  Bool_t logx, logy;
  gxT = gx;
  gyT = gy;
  logx = false;
  logy = false;
  //
  if(gx=="x")   { gxT="x";     logx=true;  }
  if(gx=="q2")  { gxT="Q^{2}"; logx=true;  }
  if(gx=="eta") { gxT="#eta";  logx=false; }
  if(gx=="p")   { gxT="p";     logx=true;  }
  //
  if(gy=="x")   { gyT="x";     logy=true;  }
  if(gy=="q2")  { gyT="Q^{2}"; logy=true;  }
  if(gy=="eta") { gyT="#eta";  logy=false; }
  if(gy=="p")   { gyT="p";     logy=true;  }

  // file names and bin vars
  std::vector<std::pair<TString,TFile*>> infiles = {
    {title0, new TFile(infile0)},
    {title1, new TFile(infile1)},
    {title2, new TFile(infile2)},
    {title3, new TFile(infile3)}
  };
  bool first=true;
  Int_t numXbins, numYbins;
  Double_t xMin, xMax, yMin, yMax;

  // PostProcessor and DAG pointers
  // - `P0` stores the first infile's DAG, which will be used for execution
  // - Dext will store the additional DAGs from other infiles
  PostProcessor *P0;
  HistosDAG *D;
  std::vector<HistosDAG*> Dext; // additional DAGs

  // get DAGs and binning
  for(auto [title,infile] : infiles) {
    D = new HistosDAG();
    D->BuildFromFile(infile);
    auto xBins = D->GetBinSet(gx);
    auto yBins = D->GetBinSet(gy);
    if(first) {
      first = false;
      P0 = new PostProcessor(infile->GetName(),outfile);
      numXbins = xBins->GetNumBins();
      numYbins = yBins->GetNumBins();
      xMin = xBins->GetMin();
      xMax = xBins->GetMax();
      yMin = yBins->GetMin();
      yMax = yBins->GetMax();
    } else {
      Dext.push_back(D);
      // check binning is the same for all DAGs
      if(numXbins != xBins->GetNumBins() || numYbins != yBins->GetNumBins()) {
        cerr << "ERROR: files have differing bins" << endl;
        return;
      }
    }
  }

  // set legend labels
  for(auto [title,infile] : infiles)
    P0->legendLabels.push_back(title);

  // 3D array structure: list of 2D arrays of Histos pointers
  // - each element of the list will be compared
  // - the 2D dimensions are the plot grid dimensions
  int numFiles = infiles.size();
  std::vector<std::vector<std::vector<Histos*>>> histosArrList(
      numFiles,
      std::vector<std::vector<Histos*>>(
        numXbins,
        std::vector<Histos*>(numYbins)
        )
      );


  // operators ====================================================

  // payload: find plot grid bin, and insert into histosArrList, for each infile
  auto fillHistosArr = [&](NodePath *NP, Histos *H ) {
    Int_t bx = NP->GetBinNode(gx)->GetBinNum();
    Int_t by = NP->GetBinNode(gy)->GetBinNum();
    Int_t pc=0;
    printf("   bx, by = %d, %d\n",bx,by);
    try { 
      histosArrList.at(0).at(bx).at(by) = H; // first, insert the Histos* of D0
      for(auto De : Dext) { // then insert the Histos* of each DAG in Dext
        histosArrList.at(++pc).at(bx).at(by) = De->GetPayloadData(NP,true); // "true", since `NP` is not owned by `P0->Op()`
      }
    }
    catch(const std::out_of_range &e) { 
      fprintf(stderr,"ERROR: invalid bin number (pc,bx,by) = (%d,%d,%d)\n",pc,bx,by);
    }
  };

  // after subloop operator: draw plot grid
  auto drawHistosArr = [&](NodePath *NP) {
    TString canvName = gx + "_" + gy + "_cov_" + NP->BinListName();
    for( TString histName : histList ) {
      P0->DrawInBins(
          canvName, histosArrList, histName,
          gxT, numXbins, xMin, xMax, logx,
          gyT, numYbins, yMin, yMax, logy,
          true, true, true
          );
    };
  };

  // staging and execution =========================================
  P0->Op()->AfterSubloop({gx,gy},drawHistosArr);
  P0->Op()->Payload(fillHistosArr);
  P0->Execute();
  P0->Finish();
};
