/*!
 *  \file		PHG4TrackFastSimEval.cc
 *  \brief		Evaluation module for PHG4TrackFastSim output
 *  \details	input: PHG4TruthInfoContainer, SvtxTrackMap with SvtxTrack_FastSim inside
 *  \author		Haiwang Yu <yuhw@nmsu.edu>
 */

#include "PHG4TrackFastSimEval.h"

#include <trackbase_historic/SvtxTrack.h>
#include <trackbase_historic/SvtxTrackMap.h>
#include <trackbase_historic/SvtxTrack_FastSim.h>
#include <trackbase_historic/SvtxVertex.h>  // for SvtxVertex
#include <trackbase_historic/SvtxVertexMap.h>

#include <g4main/PHG4Hit.h>
#include <g4main/PHG4HitContainer.h>
#include <g4main/PHG4Particle.h>
#include <g4main/PHG4TruthInfoContainer.h>
#include <g4main/PHG4VtxPoint.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/PHTFileServer.h>
#include <fun4all/SubsysReco.h>  // for SubsysReco

#include <phool/getClass.h>
#include <phool/phool.h>

#include <TH2.h>
#include <TSystem.h>
#include <TTree.h>
#include <TVector3.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <map>      // for _Rb_tree_const_ite...
#include <utility>  // for pair

#define LogDebug(exp) std::cout << "DEBUG: " << __FILE__ << ": " << __LINE__ << ": " << exp << "\n"
#define LogError(exp) std::cout << "ERROR: " << __FILE__ << ": " << __LINE__ << ": " << exp << "\n"
#define LogWarning(exp) std::cout << "WARNING: " << __FILE__ << ": " << __LINE__ << ": " << exp << "\n"

using namespace std;

//----------------------------------------------------------------------------//
//-- Constructor:
//--  simple initialization
//----------------------------------------------------------------------------//
PHG4TrackFastSimEval::PHG4TrackFastSimEval(const string &name, const string &filename, const string &trackmapname)
  : SubsysReco(name)
  , m_TruthInfoContainer(nullptr)
  , m_TrackMap(nullptr)
  , m_VertexMap(nullptr)
  , m_TracksEvalTree(nullptr)
  , m_VertexEvalTree(nullptr)
  , m_H2D_DeltaMomVsTruthMom(nullptr)
  , m_H2D_DeltaMomVsTruthEta(nullptr)
  , m_EventCounter(0)
  , m_OutFileName(filename)
  , m_TrackMapName(trackmapname)
{
  reset_variables();
}

//----------------------------------------------------------------------------//
//-- Init():
//--   Intialize all histograms, trees, and ntuples
//----------------------------------------------------------------------------//
int PHG4TrackFastSimEval::Init(PHCompositeNode *topNode)
{
  cout << PHWHERE << " Openning file " << m_OutFileName << endl;
  PHTFileServer::get().open(m_OutFileName, "RECREATE");

  // create TTree
  m_TracksEvalTree = new TTree("tracks", "FastSim Eval => tracks");
  m_TracksEvalTree->Branch("event", &m_TTree_Event, "event/I");
  m_TracksEvalTree->Branch("gtrackID", &m_TTree_gTrackID, "gtrackID/I");
  m_TracksEvalTree->Branch("gflavor", &m_TTree_gFlavor, "gflavor/I");
  m_TracksEvalTree->Branch("gpx", &m_TTree_gpx, "gpx/F");
  m_TracksEvalTree->Branch("gpy", &m_TTree_gpy, "gpy/F");
  m_TracksEvalTree->Branch("gpz", &m_TTree_gpz, "gpz/F");
  m_TracksEvalTree->Branch("gvx", &m_TTree_gvx, "gvx/F");
  m_TracksEvalTree->Branch("gvy", &m_TTree_gvy, "gvy/F");
  m_TracksEvalTree->Branch("gvz", &m_TTree_gvz, "gvz/F");
  m_TracksEvalTree->Branch("gvt", &m_TTree_gvt, "gvt/F");
  m_TracksEvalTree->Branch("trackID", &m_TTree_TrackID, "trackID/I");
  m_TracksEvalTree->Branch("charge", &m_TTree_Charge, "charge/I");
  m_TracksEvalTree->Branch("nhits", &m_TTree_nHits, "nhits/I");
  m_TracksEvalTree->Branch("px", &m_TTree_px, "px/F");
  m_TracksEvalTree->Branch("py", &m_TTree_py, "py/F");
  m_TracksEvalTree->Branch("pz", &m_TTree_pz, "pz/F");
  m_TracksEvalTree->Branch("pcax", &m_TTree_pcax, "pcax/F");
  m_TracksEvalTree->Branch("pcay", &m_TTree_pcay, "pcay/F");
  m_TracksEvalTree->Branch("pcaz", &m_TTree_pcaz, "pcaz/F");
  m_TracksEvalTree->Branch("dca2d", &m_TTree_dca2d, "dca2d/F");
  const string xyz[3] = {"x", "y", "z"};
  for (map<string, int>::const_iterator iter = m_ProjectionNameMap.begin(); iter != m_ProjectionNameMap.end(); ++iter)
  {
    string bname;
    string bdef;
    for (int i = 0; i < 3; i++)
    {
      bname = iter->first + "_" + xyz[i];
      bdef = bname +"/F";
      m_TracksEvalTree->Branch(bname.c_str(), &m_TTree_ref[i][iter->second], bdef.c_str());
    }
    for (int i = 0; i < 3; i++)
    {
      bname = iter->first + "_p" + xyz[i];
      bdef = bname +"/F";

      m_TracksEvalTree->Branch(bname.c_str(), &m_TTree_ref_p[i][iter->second], bdef.c_str());
    }
    for (int i = 0; i < 3; i++)
    {
      bname = iter->first + "_proj_" + xyz[i];
      bdef = bname +"/F";
      m_TracksEvalTree->Branch(bname.c_str(), &m_TTree_proj[i][iter->second], bdef.c_str());
    }
    for (int i = 0; i < 3; i++)
    {
      bname = iter->first + "_proj_p" + xyz[i];
      bdef = bname +"/F";
      m_TracksEvalTree->Branch(bname.c_str(), &m_TTree_proj_p[i][iter->second], bdef.c_str());
    }
  }

  m_H2D_DeltaMomVsTruthEta = new TH2D("DeltaMomVsTruthEta",
                                      "#frac{#Delta p}{truth p} vs. truth #eta", 54, -4.5, +4.5, 1000, -1,
                                      1);

  m_H2D_DeltaMomVsTruthMom = new TH2D("DeltaMomVsTruthMom",
                                      "#frac{#Delta p}{truth p} vs. truth p", 41, -0.5, 40.5, 1000, -1,
                                      1);

  // create TTree - vertex
  m_VertexEvalTree = new TTree("vertex", "FastSim Eval => vertces");
  m_VertexEvalTree->Branch("event", &m_TTree_Event, "event/I");
  m_VertexEvalTree->Branch("gvx", &m_TTree_gvx, "gvx/F");
  m_VertexEvalTree->Branch("gvy", &m_TTree_gvy, "gvy/F");
  m_VertexEvalTree->Branch("gvz", &m_TTree_gvz, "gvz/F");
  m_VertexEvalTree->Branch("gvt", &m_TTree_gvt, "gvt/F");
  m_VertexEvalTree->Branch("vx", &m_TTree_vx, "vx/F");
  m_VertexEvalTree->Branch("vy", &m_TTree_vy, "vy/F");
  m_VertexEvalTree->Branch("vz", &m_TTree_vz, "vz/F");
  m_VertexEvalTree->Branch("deltavx", &m_TTree_DeltaVx, "deltavx/F");
  m_VertexEvalTree->Branch("deltavy", &m_TTree_DeltaVy, "deltavy/F");
  m_VertexEvalTree->Branch("deltavz", &m_TTree_DeltaVz, "deltavz/F");
  m_VertexEvalTree->Branch("gID", &m_TTree_gTrackID, "gID/I");
  m_VertexEvalTree->Branch("ID", &m_TTree_TrackID, "ID/I");
  m_VertexEvalTree->Branch("ntracks", &m_TTree_nTracks, "ntracks/I");
  m_VertexEvalTree->Branch("n_from_truth", &m_TTree_nFromTruth, "n_from_truth/I");

  return Fun4AllReturnCodes::EVENT_OK;
}

//----------------------------------------------------------------------------//
//-- process_event():
//--   Call user instructions for every event.
//--   This function contains the analysis structure.
//----------------------------------------------------------------------------//
int PHG4TrackFastSimEval::process_event(PHCompositeNode *topNode)
{
  m_EventCounter++;
  if (Verbosity() >= 2 and m_EventCounter % 1000 == 0)
    cout << PHWHERE << "Events processed: " << m_EventCounter << endl;

  //std::cout << "Opening nodes" << std::endl;
  GetNodes(topNode);

  //std::cout << "Filling trees" << std::endl;
  fill_track_tree(topNode);
  fill_vertex_tree(topNode);
  //std::cout << "DONE" << std::endl;

  return Fun4AllReturnCodes::EVENT_OK;
}

//----------------------------------------------------------------------------//
//-- End():
//--   End method, wrap everything up
//----------------------------------------------------------------------------//
int PHG4TrackFastSimEval::End(PHCompositeNode *topNode)
{
  PHTFileServer::get().cd(m_OutFileName);

  m_TracksEvalTree->Write();
  m_VertexEvalTree->Write();

  m_H2D_DeltaMomVsTruthEta->Write();
  m_H2D_DeltaMomVsTruthMom->Write();

  //PHTFileServer::get().close();

  return Fun4AllReturnCodes::EVENT_OK;
}

//----------------------------------------------------------------------------//
//-- fill_tree():
//--   Fill the trees with truth, track fit, and cluster information
//----------------------------------------------------------------------------//
void PHG4TrackFastSimEval::fill_track_tree(PHCompositeNode *topNode)
{
  // Make sure to reset all the TTree variables before trying to set them.

  if (!m_TruthInfoContainer)
  {
    LogError("m_TruthInfoContainer not found!");
    return;
  }

  if (!m_TrackMap)
  {
    LogError("m_TrackMap not found!");
    return;
  }

  PHG4TruthInfoContainer::ConstRange range =
      m_TruthInfoContainer->GetPrimaryParticleRange();
  //std::cout << "A2" << std::endl;
  for (PHG4TruthInfoContainer::ConstIterator truth_itr = range.first;
       truth_itr != range.second; ++truth_itr)
  {
    reset_variables();
    //std::cout << "A1" << std::endl;
    m_TTree_Event = m_EventCounter;

    PHG4Particle *g4particle = truth_itr->second;
    if (!g4particle)
    {
      LogDebug("");
      continue;
    }
    //std::cout << "B1" << std::endl;

    SvtxTrack_FastSim *track = nullptr;

    //std::cout << "TRACKmap size " << m_TrackMap->size() << std::endl;
    for (SvtxTrackMap::ConstIter track_itr = m_TrackMap->begin();
         track_itr != m_TrackMap->end();
         track_itr++)
    {
      //std::cout << "TRACK * " << track_itr->first << std::endl;
      SvtxTrack_FastSim *temp = dynamic_cast<SvtxTrack_FastSim *>(track_itr->second);
      if (!temp)
      {
        std::cout << "ERROR CASTING PARTICLE!" << std::endl;
        continue;
      }
      //std::cout << " PARTICLE!" << std::endl;

      if ((temp->get_truth_track_id() - g4particle->get_track_id()) == 0)
      {
        track = temp;
      }
    }

    //std::cout << "B2" << std::endl;
    m_TTree_gTrackID = g4particle->get_track_id();
    m_TTree_gFlavor = g4particle->get_pid();

    m_TTree_gpx = g4particle->get_px();
    m_TTree_gpy = g4particle->get_py();
    m_TTree_gpz = g4particle->get_pz();

    m_TTree_gvx = NAN;
    m_TTree_gvy = NAN;
    m_TTree_gvz = NAN;
    m_TTree_gvt = NAN;
    PHG4VtxPoint *vtx = m_TruthInfoContainer->GetVtx(g4particle->get_vtx_id());
    if (vtx)
    {
      m_TTree_gvx = vtx->get_x();
      m_TTree_gvy = vtx->get_y();
      m_TTree_gvz = vtx->get_z();
      m_TTree_gvt = vtx->get_t();
    }

    if (track)
    {
      //std::cout << "C1" << std::endl;
      m_TTree_TrackID = track->get_id();
      m_TTree_Charge = track->get_charge();
      m_TTree_nHits = track->size_clusters();

      m_TTree_px = track->get_px();
      m_TTree_py = track->get_py();
      m_TTree_pz = track->get_pz();
      m_TTree_pcax = track->get_x();
      m_TTree_pcay = track->get_y();
      m_TTree_pcaz = track->get_z();
      m_TTree_dca2d = track->get_dca2d();

      TVector3 truth_mom(m_TTree_gpx, m_TTree_gpy, m_TTree_gpz);
      TVector3 reco_mom(m_TTree_px, m_TTree_py, m_TTree_pz);
      //std::cout << "C2" << std::endl;

      m_H2D_DeltaMomVsTruthMom->Fill(truth_mom.Mag(), (reco_mom.Mag() - truth_mom.Mag()) / truth_mom.Mag());
      m_H2D_DeltaMomVsTruthEta->Fill(truth_mom.Eta(), (reco_mom.Mag() - truth_mom.Mag()) / truth_mom.Mag());
      // find projections
      for (SvtxTrack::ConstStateIter trkstates = track->begin_states();
           trkstates != track->end_states();
           ++trkstates)
      {
        //	cout << "checking " << trkstates->second->get_name() << endl;
        map<string, int>::const_iterator iter = m_ProjectionNameMap.find(trkstates->second->get_name());
        if (iter != m_ProjectionNameMap.end())
        {
          //	  cout << "found " << trkstates->second->get_name() << endl;
          // setting the projection (xyz and pxpypz)
          m_TTree_proj[0][iter->second] = trkstates->second->get_x();
          m_TTree_proj[1][iter->second] = trkstates->second->get_y();
          m_TTree_proj[2][iter->second] = trkstates->second->get_z();
          m_TTree_proj_p[0][iter->second] = trkstates->second->get_px();
          m_TTree_proj_p[1][iter->second] = trkstates->second->get_py();
          m_TTree_proj_p[2][iter->second] = trkstates->second->get_pz();

          string nodename = "G4HIT_" + trkstates->second->get_name();
          PHG4HitContainer *hits = findNode::getClass<PHG4HitContainer>(topNode, nodename);
          if (!hits)
          {
            cout << "could not find " << nodename << endl;
            continue;
          }
          //	  cout << "number of hits: " << hits->size() << endl;
          PHG4HitContainer::ConstRange hit_range = hits->getHits();
          for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
          {
            //	    cout << "checking hit id " << hit_iter->second->get_trkid() << " against " << track->get_truth_track_id() << endl;
            if (hit_iter->second->get_trkid() - track->get_truth_track_id() == 0)
            {
              //	      cout << "found hit with id " << hit_iter->second->get_trkid() << endl;
              if (iter->second >= m_MaxNumberProjections)
              {
                cout << "bad index: " << iter->second << endl;
                gSystem->Exit(1);
              }
              m_TTree_ref[0][iter->second] = hit_iter->second->get_x(0);
              m_TTree_ref[1][iter->second] = hit_iter->second->get_y(0);
              m_TTree_ref[2][iter->second] = hit_iter->second->get_z(0);
              m_TTree_ref_p[0][iter->second] = hit_iter->second->get_px(0);
              m_TTree_ref_p[1][iter->second] = hit_iter->second->get_py(0);
              m_TTree_ref_p[2][iter->second] = hit_iter->second->get_pz(0);
            }
          }
        }
      }
    }
    //std::cout << "B3" << std::endl;

    m_TracksEvalTree->Fill();
  }
  //std::cout << "A3" << std::endl;

  return;
}

//----------------------------------------------------------------------------//
//-- fill_tree():
//--   Fill the trees with truth, track fit, and cluster information
//----------------------------------------------------------------------------//
void PHG4TrackFastSimEval::fill_vertex_tree(PHCompositeNode *topNode)
{
  if (!m_TruthInfoContainer)
  {
    LogError("m_TruthInfoContainer not found!");
    return;
  }

  if (!m_TrackMap)
  {
    LogError("m_TrackMap not found!");
    return;
  }

  if (!m_VertexMap)
  {
    return;
  }

  for (SvtxVertexMap::Iter iter = m_VertexMap->begin();
       iter != m_VertexMap->end();
       ++iter)
  {
    SvtxVertex *vertex = iter->second;

    // Make sure to reset all the TTree variables before trying to set them.
    reset_variables();
    //std::cout << "A1" << std::endl;
    m_TTree_Event = m_EventCounter;

    if (!vertex)
    {
      LogDebug("");
      continue;
    }

    //std::cout << "C1" << std::endl;
    m_TTree_TrackID = vertex->get_id();
    m_TTree_nTracks = vertex->size_tracks();

    m_TTree_vx = vertex->get_x();
    m_TTree_vy = vertex->get_y();
    m_TTree_vz = vertex->get_z();
    m_TTree_DeltaVx = sqrt(vertex->get_error(1, 1));
    m_TTree_DeltaVy = sqrt(vertex->get_error(2, 2));
    m_TTree_DeltaVz = sqrt(vertex->get_error(3, 3));

    // best matched vertex
    PHG4VtxPoint *best_vtx = nullptr;
    int best_n_match = -1;
    map<PHG4VtxPoint *, int> vertex_match_map;
    for (auto iter = vertex->begin_tracks(); iter != vertex->end_tracks(); ++iter)
    {
      const auto &trackid = *iter;
      const auto trackIter = m_TrackMap->find(trackid);

      if (trackIter == m_TrackMap->end()) continue;

      SvtxTrack_FastSim *temp = dynamic_cast<SvtxTrack_FastSim *>(trackIter->second);

      if (!temp) continue;

      const auto g4trackID = temp->get_truth_track_id();
      const PHG4Particle *g4particle = m_TruthInfoContainer->GetParticle(g4trackID);
      assert(g4particle);
      PHG4VtxPoint *vtx = m_TruthInfoContainer->GetVtx(g4particle->get_vtx_id());

      int n_match = ++vertex_match_map[vtx];

      if (n_match > best_n_match)
      {
        best_n_match = n_match;
        best_vtx = vtx;
      }
    }
    if (best_vtx)
    {
      m_TTree_gvx = best_vtx->get_x();
      m_TTree_gvy = best_vtx->get_y();
      m_TTree_gvz = best_vtx->get_z();
      m_TTree_gvt = best_vtx->get_t();

      m_TTree_nFromTruth = best_n_match;
      m_TTree_gTrackID = best_vtx->get_id();
    }
    m_VertexEvalTree->Fill();
  }
  //std::cout << "B3" << std::endl;

  return;
}

//----------------------------------------------------------------------------//
//-- reset_variables():
//--   Reset all the tree variables to their default values.
//--   Needs to be called at the start of every event
//----------------------------------------------------------------------------//
void PHG4TrackFastSimEval::reset_variables()
{
  m_TTree_Event = -9999;

  //-- truth
  m_TTree_gTrackID = -9999;
  m_TTree_gFlavor = -9999;
  m_TTree_gpx = NAN;
  m_TTree_gpy = NAN;
  m_TTree_gpz = NAN;

  m_TTree_gvx = NAN;
  m_TTree_gvy = NAN;
  m_TTree_gvz = NAN;
  m_TTree_gvt = NAN;

  //-- reco
  m_TTree_TrackID = -9999;
  m_TTree_Charge = -9999;
  m_TTree_nHits = -9999;
  m_TTree_px = NAN;
  m_TTree_py = NAN;
  m_TTree_pz = NAN;
  m_TTree_pcax = NAN;
  m_TTree_pcay = NAN;
  m_TTree_pcaz = NAN;
  m_TTree_dca2d = NAN;

  m_TTree_vx = NAN;
  m_TTree_vy = NAN;
  m_TTree_vz = NAN;
  m_TTree_DeltaVx = NAN;
  m_TTree_DeltaVy = NAN;
  m_TTree_DeltaVz = NAN;
  m_TTree_nTracks = -9999;
  m_TTree_nFromTruth = -9999;
  // projections
  for (int k = 0; k < 3; k++)
  {
    for (int j = 0; j < m_MaxNumberProjections; j++)
    {
      m_TTree_proj[k][j] = -9999;
      m_TTree_proj_p[k][j] = -9999;
      m_TTree_ref[k][j] = -9999;
      m_TTree_ref_p[k][j] = -9999;
    }
  }
}

//----------------------------------------------------------------------------//
//-- GetNodes():
//--   Get all the all the required nodes off the node tree
//----------------------------------------------------------------------------//
int PHG4TrackFastSimEval::GetNodes(PHCompositeNode *topNode)
{
  //DST objects
  //Truth container
  m_TruthInfoContainer = findNode::getClass<PHG4TruthInfoContainer>(topNode,
                                                                    "G4TruthInfo");
  if (!m_TruthInfoContainer && m_EventCounter < 2)
  {
    cout << PHWHERE << " PHG4TruthInfoContainer node not found on node tree"
         << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_TrackMap = findNode::getClass<SvtxTrackMap>(topNode,
                                                m_TrackMapName);
  //std::cout << m_TrackMapName << std::endl;
  if (!m_TrackMap)
  {
    cout << PHWHERE << "SvtxTrackMap node with name "
         << m_TrackMapName
         << " not found on node tree"
         << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_VertexMap = findNode::getClass<SvtxVertexMap>(topNode, "SvtxVertexMap");
  if (!m_VertexMap && Verbosity())
  {
    cout << PHWHERE << "SvtxTrackMap node with name SvtxVertexMap not found on node tree. Will not build the vertex eval tree"
         << endl;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

void PHG4TrackFastSimEval::AddProjection(const string &name)
{
  unsigned int size = m_ProjectionNameMap.size();
  if (size >= m_MaxNumberProjections)
  {
    cout << "Too many projections in evaluator, maximum number is "
         << m_MaxNumberProjections << endl;
    cout << "Cannot add " << name << endl;
    cout << "increase m_MaxNumberProjections in PHG4TrackFastSimEval, recompile and rerun" << endl;
    gSystem->Exit(1);
  }
  m_ProjectionNameMap.insert(make_pair(name, m_ProjectionNameMap.size()));
  return;
}
