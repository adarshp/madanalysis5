////////////////////////////////////////////////////////////////////////////////
//  
//  Copyright (C) 2012-2016 Eric Conte, Benjamin Fuks
//  The MadAnalysis development team, email: <ma5team@iphc.cnrs.fr>
//  
//  This file is part of MadAnalysis 5.
//  Official website: <https://launchpad.net/madanalysis5>
//  
//  MadAnalysis 5 is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//  
//  MadAnalysis 5 is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with MadAnalysis 5. If not, see <http://www.gnu.org/licenses/>
//  
////////////////////////////////////////////////////////////////////////////////


#include "SampleAnalyzer/Commons/Service/Isolation.h"
using namespace MA5;


// -----------------------------------------------------------------------------
// JetCleaning
// -----------------------------------------------------------------------------
std::vector<const RecJetFormat*>
Isolation::JetCleaning(const std::vector<const RecJetFormat*>& uncleaned_jets,
                            const std::vector<const RecLeptonFormat*>& leptons,
                            double DeltaRmax, double PTmin) const
{
  // cleaned collection of jets
  std::vector<const RecJetFormat*> cleaned_jets;

  // mask for jets
  // -> true = to remove
  std::vector<bool> mask(uncleaned_jets.size(),false);

  // apply the cut on PT
  for (unsigned int i=0;i<uncleaned_jets.size();i++)
  {
    if (uncleaned_jets[i]->pt()<PTmin) mask[i]=true;
  }

  for (unsigned int i=0;i<leptons.size();i++)
  {
    // safety
    if (leptons[i]==0) continue;
    if (leptons[i]->pt()<1e-6) continue;

    //
    int jet_index=-1;
    double deltaR_min=0;

    // loop over jtes
    for (unsigned int j=0;j<uncleaned_jets.size();j++)
    {
      // shortcut to jet
      const RecJetFormat& jet = *(uncleaned_jets[j]);

      // mask: the lepton has been already removed
      if (mask[j]) continue;

      // cut on DeltaR
      double dr = leptons[i]->momentum().DeltaR(jet.momentum());
      if (dr>DeltaRmax) continue;

      // is it the closest lepton to the jet?
      if (jet_index==-1 || dr<deltaR_min)
      {
        deltaR_min=dr;
        jet_index=j;
      }
    }

    // is a lepton matched the jet?
    if (jet_index!=-1)
    {
      mask[jet_index]=true;
    }

  }

  // save the jets
  for (unsigned int i=0;i<uncleaned_jets.size();i++)
  {
    if (!mask[i]) cleaned_jets.push_back(uncleaned_jets[i]);
  }

  // return the cleaned collection
  return cleaned_jets;
}

// -----------------------------------------------------------------------------
// JetCleaning
// -----------------------------------------------------------------------------
std::vector<const RecJetFormat*>
Isolation::JetCleaning(const std::vector<const RecJetFormat*>& uncleaned_jets,
                            const std::vector<const RecPhotonFormat*>& photons,
                            double DeltaRmax, double PTmin) const
{
  // cleaned collection of jets
  std::vector<const RecJetFormat*> cleaned_jets;

  // mask for jets
  // -> true = to remove
  std::vector<bool> mask(uncleaned_jets.size(),false);

  // apply the cut on PT
  for (unsigned int i=0;i<uncleaned_jets.size();i++)
  {
    if (uncleaned_jets[i]->pt()<PTmin) mask[i]=true;
  }

  // loop over photons
  for (unsigned int i=0;i<photons.size();i++)
  {
    // safety
    if (photons[i]==0) continue;
    if (photons[i]->pt()<1e-6) continue;

    //
    int jet_index=-1;
    double deltaR_min=0;

    // loop over jtes
    for (unsigned int j=0;j<uncleaned_jets.size();j++)
    {
      // shortcut to jet
      const RecJetFormat& jet = *(uncleaned_jets[j]);

      // mask: the photon has been already removed
      if (mask[j]) continue;

      // cut on DeltaR
      double dr = photons[i]->momentum().DeltaR(jet.momentum());
      if (dr>DeltaRmax) continue;

      // is it the closest photon to the jet?
      if (jet_index==-1 || dr<deltaR_min)
      {
        deltaR_min=dr;
        jet_index=j;
      }
    }

    // is a photon matched the jet?
    if (jet_index!=-1)
    {
      mask[jet_index]=true;
    }

  }

  // save the jets
  for (unsigned int i=0;i<uncleaned_jets.size();i++)
  {
    if (!mask[i]) cleaned_jets.push_back(uncleaned_jets[i]);
  }

  // return the cleaned collection
  return cleaned_jets;
}

