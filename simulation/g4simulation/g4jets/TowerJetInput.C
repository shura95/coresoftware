
#include "TowerJetInput.h"

#include "JetInput.h"
#include "Jet.h"
#include "JetV1.h"

#include <phool/PHNodeIterator.h>
#include <phool/PHTypedNodeIterator.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/getClass.h>

#include <g4cemc/RawTowerGeomContainer.h>
#include <g4cemc/RawTowerContainer.h>
#include <g4cemc/RawTowerGeom.h>
#include <g4cemc/RawTower.h>
#include <g4vertex/GlobalVertexMap.h>
#include <g4vertex/GlobalVertex.h>

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cassert>

using namespace std;

TowerJetInput::TowerJetInput(Jet::SRC input)
  : _verbosity(0),
    _input(input) {
}

void TowerJetInput::identify(std::ostream& os) {
  os << "   TowerJetInput: ";
  if      (_input == Jet::CEMC_TOWER)    os << "TOWER_CEMC to Jet::CEMC_TOWER";
  else if (_input == Jet::HCALIN_TOWER)  os << "TOWER_HCALIN to Jet::HCALIN_TOWER";
  else if (_input == Jet::HCALOUT_TOWER) os << "TOWER_HCALOUT to Jet::HCALOUT_TOWER";
  else if (_input == Jet::FEMC_TOWER) os << "TOWER_FEMC to Jet::FEMC_TOWER";
  else if (_input == Jet::FHCAL_TOWER) os << "TOWER_FHCAL to Jet::FHCAL_TOWER";
  os << endl;
}

std::vector<Jet*> TowerJetInput::get_input(PHCompositeNode *topNode) {
  
  if (_verbosity > 0) cout << "TowerJetInput::process_event -- entered" << endl;

  GlobalVertexMap* vertexmap = findNode::getClass<GlobalVertexMap>(topNode,"GlobalVertexMap");
  if (!vertexmap) {

    cout <<"TowerJetInput::get_input - Fatal Error - GlobalVertexMap node is missing. Please turn on the do_global flag in the main macro in order to reconstruct the global vertex."<<endl;
    assert(vertexmap); // force quit

    return std::vector<Jet*>();
  }

  RawTowerContainer *towers = NULL;
  RawTowerGeomContainer *geom = NULL;
  if (_input == Jet::CEMC_TOWER) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_CEMC");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_CEMC");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::HCALIN_TOWER) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_HCALIN");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_HCALIN");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::HCALOUT_TOWER) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_HCALOUT");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_HCALOUT");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::FEMC_TOWER) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_FEMC");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_FEMC");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::FHCAL_TOWER) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_FHCAL");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_FHCAL");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::CEMC_TOWER_SUB1) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_CEMC_RETOWER_SUB1");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_HCALIN");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::HCALIN_TOWER_SUB1) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_HCALIN_SUB1");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_HCALIN");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else if (_input == Jet::HCALOUT_TOWER_SUB1) {
    towers = findNode::getClass<RawTowerContainer>(topNode,"TOWER_CALIB_HCALOUT_SUB1");
    geom = findNode::getClass<RawTowerGeomContainer>(topNode,"TOWERGEOM_HCALOUT");
    if (!towers||!geom) {
      return std::vector<Jet*>();
    }
  } else {
    return std::vector<Jet*>();
  }

  // first grab the event vertex or bail
  GlobalVertex* vtx = vertexmap->begin()->second;
  float vtxz = NAN;
  if (vtx) vtxz = vtx->get_z();
  else return std::vector<Jet*>();

  if (isnan(vtxz))
    {
      static bool once = true;
      if (once)
        {
          once = false;

          cout <<"TowerJetInput::get_input - WARNING - vertex is NAN. Drop all tower inputs (further NAN-vertex warning will be suppressed)."<<endl;
        }

      return std::vector<Jet*>();
    }

  std::vector<Jet*> pseudojets;
  RawTowerContainer::ConstRange begin_end = towers->getTowers();
  RawTowerContainer::ConstIterator rtiter;
  for (rtiter = begin_end.first; rtiter !=  begin_end.second; ++rtiter) {
    RawTower *tower = rtiter->second;

    RawTowerGeom * tower_geom =
    geom->get_tower_geometry(tower -> get_key());
    assert(tower_geom);

    double r = tower_geom->get_center_radius();
    double phi = atan2(tower_geom->get_center_y(), tower_geom->get_center_x());
    double z0 = tower_geom->get_center_z();

    double z = z0 - vtxz;
    
    double eta = asinh(z/r); // eta after shift from vertex

    double pt = tower->get_energy() / cosh(eta);
    double px = pt * cos(phi);
    double py = pt * sin(phi);
    double pz = pt * sinh(eta);

    Jet *jet = new JetV1();
    jet->set_px(px);
    jet->set_py(py);
    jet->set_pz(pz);
    jet->set_e(tower->get_energy());
    jet->insert_comp(_input,tower->get_id());

    pseudojets.push_back(jet);
  }

  if (_verbosity > 0) cout << "TowerJetInput::process_event -- exited" << endl;

  return pseudojets;
}
