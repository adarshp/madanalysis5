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


// SampleHeader headers
#include "SampleAnalyzer/Interfaces/root/RootMainHeaders.h"



// Relations between TLorentzVector & MALorentzVector
TLorentzVector ToTLorentzVector(const MA5::MALorentzVector& a)
{
  return TLorentzVector(a.Px(),a.Py(),a.Pz(),a.E());
}
