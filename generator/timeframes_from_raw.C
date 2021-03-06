/// @file   timeframes_from_raw.C
/// @author Matthias.Richter@scieq.net
/// @date   2015-07-09
/// @brief  
///
///
/// Changelog:

// for performance reasons, the macro is compiled into a temporary library
// and the compiled function is called
// Note: when using new classes the corresponding header files need to be
// add in the include section

#if defined(__CINT__) && !defined(__MAKECINT__)
{
  gSystem->AddIncludePath("-I$ROOTSYS/include -I$ALICE_ROOT/include -I.");
  TString macroname=gInterpreter->GetCurrentMacroName();
  macroname+="+";
  gSystem->Load("libGenerator.so");
  if (gSystem->DynFindSymbol("Generator", "__IsChannelMergerIncludedInLibrary") == NULL)
    gROOT->LoadMacro("ChannelMerger.cxx+");
  gROOT->LoadMacro(macroname);
  // running parameters can be changed by adjusting the default parameters
  // of the function definition below
  timeframes_from_raw();
}
#else
#include "GeneratorTF.h"
#include "ChannelMerger.h"
#include <vector>
#include <iostream>
#include <fstream>
#include "TTree.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TSystem.h"
#include "AliHLTHuffman.h"

void timeframes_from_raw(const int   g_pileupmode=3, // 0 - fixed number of collisions at offset 0
                                                     // 1 - random number of collisions at offset 0
                                                     // 2 - fixed number of collisions at random offset (not yet supported)
                                                     // 3 - random number of collisions at random offset
                         const float g_rate=5.,      // avrg rate with respect to unit time, i.e. framesize
                         const int   g_ncollisions=10,  // number of collisions per frame for pileup mode 0 and 2
                         const int   g_nframes=1000,    // number of timeframes to be generated
                         const int   g_baseline=5, // place baseline at n ADC counts after pedestal subtraction
                         const int   g_thresholdZS=2, // threshold for zero suppression, this requires the pedestal configuration to make sense
                         const int   g_noiseFactor=1, // manipulation of the noise, roughly multiplying by factor
                         const int   g_doHuffmanCompression=0, // 0 - off, 1 - compression, 2 - training
                         const int   g_huffmanLengthCutoff=0, // 0 - off, >0 symbols with lenght >= cutoff are stored with a marker of length cutoff and the original value
                         const int   g_applyCommonModeEffect=0, // 0 - off, 1 = on
                         const int   g_normalizeTimeframe=0, // 0 - off, 1 - normalize each TF by the number of included collisions
                         const char* g_pedestalConfiguration="pedestal.dat", // pedestal configuration file
                         const char* g_channelMappingConfiguration="mapping.dat",
                         const char* g_confFilenames="datafiles.txt",
                         const char* g_huffmanFileName="TPCRawSignalDifference",
                         const char* g_targetFileName="tpc-raw-channel-stat.root",
                         const int   g_statisticsTreeMode=1, // 0 - off, 1 - normal, 2 - extended (including bunch length statistics)
                         const char* g_statisticsTextFileName=NULL, // write channel statistics to a text file
                         const char* g_asciiDataTargetDir=NULL,//"tfdata", // write channel data to ascii file in target directory, off if NULL
                         const char* g_systemsTargetdir=NULL,//"systemcinput", // write input files for SystemC simulation to directory, off if NULL
			 const int   g_minddl=0, // range of DDLs to be read, -1 to disable
			 const int   g_maxddl=1,
			 const int   g_minpadrow=-1, // range of padrows, use -1 to disable selection, Note: this requires the mapping file for channels
			 const int   g_maxpadrow=-1
                         )
{
  const int   ddlrange[2]={g_minddl, g_maxddl};
  const int   padrowrange[2]={g_minpadrow, g_maxpadrow};

  // signal bit length
  const int signalBitLength=10;
  const int signalRange=0x1<<signalBitLength;

  // huffman decoder settings
  const char* huffmanDecoderName=g_huffmanFileName;
  TString htfn=huffmanDecoderName;
  htfn+="_HuffmanTable.root";
  AliHLTHuffman* pHuffman=NULL;
  if (g_doHuffmanCompression==2) {
    // training mode, create new instance
    pHuffman=new AliHLTHuffman(huffmanDecoderName, signalBitLength+1);
  } else if (g_doHuffmanCompression==1) {
    // compression mode, load huffman table
    TFile* htf=TFile::Open(htfn);
    if (!htf || htf->IsZombie()) {
      cerr << "can not open file " << htfn << endl;
      cerr << "please run the macro in 'training' mode by setting bRunHuffmanTraining to true" << endl;
      return;
    }

    TObject* obj=NULL;
    htf->GetObject(g_huffmanFileName, obj);
    if (obj==NULL) {
      cout << "can not load Huffman decoder object " << huffmanDecoderName << " from file " << htfn << endl;
      return;
    }
    pHuffman=(AliHLTHuffman*)obj->Clone();
    htf->Close();
    delete htf;
  }

  GeneratorTF generator(g_rate);
  ChannelMerger merger;
  if (ddlrange[0]>=0 && ddlrange[1]>=0)
    merger.SetDDLRange(ddlrange[0], ddlrange[1]);
  if (padrowrange[0]>=0 && padrowrange[1]>=0)
    merger.SetPadRowRange(padrowrange[0], padrowrange[1]);
  if (g_pedestalConfiguration)
    merger.InitChannelBaseline(g_pedestalConfiguration, -g_baseline); // note the '-'!
  if (g_channelMappingConfiguration)
    merger.InitAltroMapping(g_channelMappingConfiguration);
  if (g_thresholdZS>=0)
    merger.InitZeroSuppression(g_thresholdZS);
  merger.InitNoiseManipulation(g_noiseFactor);
  bool bHaveSignalOverflow=false;

  std::istream* inputfiles=&std::cin;
  std::ifstream inputconfiguration(g_confFilenames);
  if (inputconfiguration.good()) {
    inputfiles=&inputconfiguration;
  } else {
    std::cout << "Can not open configuration file '" << g_confFilenames << "' " << std::endl
	      << "Reading input file names from std input, one filename per line, " << std::endl
	      << "Abort the macro if nothing is provided to std input !!!" << std::endl;
  }

  // statistics analysis
  TH1* hCollisionTimes=new TH1F("hCollisionTimes", "Time difference of collisions in TF", 100, 0., 2.);
  hCollisionTimes->GetXaxis()->SetTitle("time relative to TF");
  hCollisionTimes->GetYaxis()->SetTitle("count");

  TH1* hCollisionOffset=new TH1F("hCollisionOffset", "Offset for individual collisions in TF", 100, 0., 2.);
  hCollisionOffset->GetXaxis()->SetTitle("time relative to TF");
  hCollisionOffset->GetYaxis()->SetTitle("count");

  TH1* hNCollisions=new TH1F("hNCollisions", "Number of collisions in TF", 20, 0., 20.);
  hNCollisions->GetXaxis()->SetTitle("number of collisions in TF");
  hNCollisions->GetYaxis()->SetTitle("count");

  // vaiables for the statistics tree
  int TimeFrameNo=0;
  int NCollisions=0;
  int DDLNumber=0;
  int HWAddr=0;
  int PadRow=0;
  int MinSignal=0;
  int MaxSignal=0;
  int AvrgSignal=0;
  int MinSignalDiff=0;
  int MaxSignalDiff=0;
  int MinTimebin=0;
  int MaxTimebin=0;
  int NFilledTimebins=0;
  int NBunches=0;
  int BunchLength[1]; // length is dummy, different variable used for tree filling
  float HuffmanFactor=1.;

  // due to the interface design of the ChannelMerger::Analyze function we
  // always need to set up an object passed then by reference, it can
  // be , however, empty
  TTree *channelstat=new TTree("channelstat","TPC RAW channel statistics");
  if (channelstat && g_statisticsTreeMode > 0) {
    // no branches are added if statistics tree mode is 0
    channelstat->Branch("TimeFrameNo"    , &TimeFrameNo     , "TimeFrameNo/I");
    channelstat->Branch("NCollisions"    , &NCollisions     , "NCollisions/I");
    channelstat->Branch("DDLNumber"      , &DDLNumber       , "DDLNumber/I");
    channelstat->Branch("HWAddr"         , &HWAddr          , "HWAddr/I");
    channelstat->Branch("PadRow"         , &PadRow          , "PadRow/I");
    channelstat->Branch("MinSignal"      , &MinSignal       , "MinSignal/I");
    channelstat->Branch("MaxSignal"      , &MaxSignal       , "MaxSignal/I");
    channelstat->Branch("AvrgSignal"     , &AvrgSignal      , "AvrgSignal/I");
    channelstat->Branch("MinSignalDiff"  , &MinSignalDiff   , "MinSignalDiff/I");
    channelstat->Branch("MaxSignalDiff"  , &MaxSignalDiff   , "MaxSignalDiff/I");
    channelstat->Branch("MinTimebin"     , &MinTimebin      , "MinTimebin/I");
    channelstat->Branch("MaxTimebin"     , &MaxTimebin      , "MaxTimebin/I");
    channelstat->Branch("NFilledTimebins", &NFilledTimebins , "NFilledTimebins/I");
    channelstat->Branch("NBunches"       , &NBunches        , "NBunches/I");
    if (g_statisticsTreeMode >= 2) {
      // extended statistics
      channelstat->Branch("BunchLength"    , BunchLength      , "BuncheLength[NBunches]/i");
    }
  }

  TTree *huffmanstat=NULL;

  if (g_doHuffmanCompression>0)
    huffmanstat=new TTree("huffmanstat","TPC RAW huffman compression statistics");

  if (huffmanstat) {
    huffmanstat->Branch("TimeFrameNo"    , &TimeFrameNo     , "TimeFrameNo/I");
    huffmanstat->Branch("DDLNumber"      , &DDLNumber       , "DDLNumber/I");
    huffmanstat->Branch("HWAddr"         , &HWAddr          , "HWAddr/I");
    huffmanstat->Branch("PadRow"         , &PadRow          , "PadRow/I");
    huffmanstat->Branch("NFilledTimebins", &NFilledTimebins , "NFilledTimebins/I");
    huffmanstat->Branch("HuffmanFactor"  , &HuffmanFactor   , "HuffmanFactor/F");
  }

  TH1* hHuffmanCodeLength=NULL;
  TH1* hSignalDiff=NULL;
  TH2* hHuffmanFactor=NULL;
  if (g_doHuffmanCompression>0) {
    Int_t binMargin=50; // some margin on both sides of the signal distribution
    Int_t nBins=2*(signalRange+binMargin)+1;
    hSignalDiff=new TH1D("hSignalDiff", "Differences in TPC RAW signal", nBins, -nBins/2, nBins/2);
    hSignalDiff->GetXaxis()->SetTitle("Signal(n+1) - Signal(n)");
    hSignalDiff->GetYaxis()->SetTitle("counts");
    hSignalDiff->GetYaxis()->SetTitleOffset(1.4);

    hHuffmanCodeLength=new TH1F("hHuffmanCodeLength", "Huffman code length per signal difference", nBins, -nBins/2, nBins/2);
    hHuffmanCodeLength->GetXaxis()->SetTitle("Signal(n+1) - Signal(n)");
    hHuffmanCodeLength->GetYaxis()->SetTitle("Huffman code length");
    hHuffmanCodeLength->GetYaxis()->SetTitleOffset(1.4);

    hHuffmanFactor=new TH2F("hHuffmanFactor", "Huffman Compression Factor", 61, -1, 60, 100, 0, 5);
    hHuffmanFactor->GetXaxis()->SetTitle("Padrow number");
    hHuffmanFactor->GetYaxis()->SetTitle("Huffman compression factor");
  }

  std::vector<float> singleTF;

  // backup of last collision offset for calculation of time
  // difference
  // TODO: this variable can be set according to configuration/mode of generator
  // if configuration is supported by the generator
  bool bInverseWrtTF=false; // set true if the generator produces offsets wrt end of TF
  float lastTime=0.;

  while (TimeFrameNo++<g_nframes || g_nframes<0) {
    if (g_statisticsTextFileName != NULL && TimeFrameNo > 1) {
      // statistics file is written for only one time frame, it would overwrite
      // previous frames
      break;
    }
    if (bInverseWrtTF) {
      // collision offsets are with respect to the end of timeframe
      lastTime+=1.;
    } else {
      // collision offsets are with respect to the start of timeframe
      lastTime-=1.;
    }

    std::vector<float> tf;

    if ((g_pileupmode&0x1) == 0) {
      // fixed number of collisions
      if (g_pileupmode != 0) {
	std::cerr << "fixed number of collisions at random offsets not yet supported" << std:: endl;
	return;
      }
      tf.resize(g_ncollisions, 0.);
    } else {
      // random number of collisions
      const std::vector<float>& randomTF=generator.SimulateCollisionSequence();
      if ((g_pileupmode&0x2) == 0) {
	// merge random number of collisions, each at offset 0.
	tf.resize(randomTF.size(), 0.);
      } else {
	// merge random number of collisions at random offsets
	tf=randomTF;
      }
    }

    if (hCollisionOffset || hCollisionTimes) {
      for (unsigned i=0; i<tf.size(); i++) {
	if (hCollisionOffset) hCollisionOffset->Fill(tf[i]);
	if (bInverseWrtTF) {
	if (lastTime<0.) {
	  lastTime=tf[i];
	} else {
	  lastTime-=tf[i];
	  if (hCollisionTimes) hCollisionTimes->Fill(lastTime);
	  lastTime=tf[i];
	}
	} else {
	  if (lastTime>-1.) {
	    lastTime=tf[i]-lastTime;
	    if (hCollisionTimes) hCollisionTimes->Fill(lastTime);
	  }
	  lastTime=tf[i];
	}
      }
    }
    if (hNCollisions) {
      hNCollisions->Fill(tf.size());
    }
    NCollisions=tf.size();
    merger.StartTimeframe();
    int mergedCollisions=merger.MergeCollisions(tf, *inputfiles);
    if (g_normalizeTimeframe) {
      // normalization for estimation of baseline
      // not to be used for colision pileup in timeframes
      merger.Normalize(NCollisions);
    }
    merger.CalculateZeroSuppression(g_doHuffmanCompression==0);
    if (g_applyCommonModeEffect>0)
      merger.ApplyCommonModeEffect();
    merger.Analyze(*channelstat, g_statisticsTextFileName);
    if (g_doHuffmanCompression>0) {
      merger.DoHuffmanCompression(pHuffman, g_doHuffmanCompression==2, *hHuffmanFactor, *hSignalDiff, huffmanstat, g_huffmanLengthCutoff);
    }
    if (merger.GetSignalOverflowCount() > 0) {
      std::cout << "signal overflow in current timeframe detected" << std::endl;
      bHaveSignalOverflow=true;
    }
    if (mergedCollisions < 0) {
      std::cerr << "merging collisions failed with error code " << mergedCollisions << std::endl;
      break;
    } else if (mergedCollisions != (int)tf.size()) {
      // probably no more input data to be read
      std::cout << "simulated " << TimeFrameNo-1 << " timeframe(s)" << std::endl;
      break;
    }

    if (g_asciiDataTargetDir) {
      // write timeframe data to file
      TString dirname(g_asciiDataTargetDir);
      TString command("mkdir -p "); command+=dirname;
      gSystem->Exec(command.Data());
      TString filename;
      filename.Form("%s/tf%04d.dat", dirname.Data(), TimeFrameNo-1);
      merger.WriteTimeframe(filename.Data());
    }

    if (g_systemsTargetdir != NULL) {
      // write to text file used for SystemC simulation
      TString dirname(g_systemsTargetdir);
      TString command("mkdir -p "); command+=dirname;
      gSystem->Exec(command.Data());
      TString filename;
      filename.Form("%s/event%04d.dat", dirname.Data(), TimeFrameNo-1);
      merger.WriteSystemcInputFile(filename.Data());
    }

    std::cout << "Successfully generated timeframe " << TimeFrameNo << " from " << tf.size() << " collision(s)" << std::endl;
    for (std::vector<float>::const_iterator element=tf.begin(); element!=tf.end(); element++) std::cout << "   collision at offset " << *element << std::endl;
  }
  if (bHaveSignalOverflow) {
    std::cout << "WARNING: signal overflow detected in at least one timeframe" << std::endl;
  }

  if (pHuffman && g_doHuffmanCompression==2) {
    // training mode, calculate huffman table
    pHuffman->GenerateHuffmanTree();
    pHuffman->Print();
    TFile* htf=TFile::Open(htfn, "RECREATE");
    if (!htf || htf->IsZombie()) {
      cerr << "can not open file " << htfn << endl;
      return;
    }

    htf->cd();
    pHuffman->Write();
    htf->Close();
  }

  if (pHuffman) {
    for (int iSignalDiff=-signalRange; iSignalDiff<signalRange; iSignalDiff++) {
      AliHLTUInt64_t length = 0;
      AliHLTUInt64_t v = iSignalDiff+signalRange;
      pHuffman->Encode(v, length);
      hHuffmanCodeLength->Fill(iSignalDiff, length);
    }
  }

  TFile* of=TFile::Open(g_targetFileName, "RECREATE");
  if (!of || of->IsZombie()) {
    cerr << "can not open file " << g_targetFileName << endl;
    return;
  }

  of->cd();
  if (channelstat && g_statisticsTreeMode > 0) {
    channelstat->Print();
    channelstat->Write();
  }
  if (hNCollisions)
    hNCollisions->Write();

  if (hCollisionTimes)
    hCollisionTimes->Write();

  if (hCollisionOffset)
    hCollisionOffset->Write();

  if (hHuffmanCodeLength)
    hHuffmanCodeLength->Write();

  if (hSignalDiff)
    hSignalDiff->Write();

  if (hHuffmanFactor)
    hHuffmanFactor->Write();

  if (huffmanstat) {
    huffmanstat->Print();
    huffmanstat->Write();
  }

  of->Close();
}

int main()
{
  timeframes_from_raw();
}

#endif
