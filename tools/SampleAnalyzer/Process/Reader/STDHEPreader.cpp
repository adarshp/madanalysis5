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


// SampleAnalyzer headers
#include "SampleAnalyzer/Process/Reader/STDHEPreader.h"
#include "SampleAnalyzer/Commons/Service/LogService.h"
#include "SampleAnalyzer/Commons/Service/ConvertService.h"
#include "SampleAnalyzer/Commons/Service/ExceptionService.h"


using namespace MA5;

// -----------------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------------
bool STDHEPreader::Initialize(const std::string& rawfilename,
                              const Configuration& cfg)
{
  nevhept_before_ = 0;
  firstevent=true;

  if (!ReaderTextBase::Initialize(rawfilename, cfg)) return false;
  xdrinput_=new xdr_istream(*input_);

  return true;
}


// -----------------------------------------------------------------------------
// ReadHeader
// -----------------------------------------------------------------------------
void STDHEPreader::Reset()
{
  nevhept_=0;
  nhept_=0;
  isthept_.clear();
  idhept_.clear();
  jmohept_.clear();
  jdahept_.clear();
  phept_.clear();
  vhept_.clear();
}

// -----------------------------------------------------------------------------
// ReadHeader
// -----------------------------------------------------------------------------
bool STDHEPreader::ReadHeader(SampleFormat& mySample)
{
  // Initiliaze MC
  mySample.InitializeMC();
  mySample.SetSampleFormat(MA5FORMAT::STDHEP);

  if (!DecodeFileHeader(mySample)) return false;

  return true;
}


// -----------------------------------------------------------------------------
// ReadEvent
// -----------------------------------------------------------------------------
StatusCode::Type STDHEPreader::ReadEvent(EventFormat& myEvent, SampleFormat& mySample)
{
  // Initiliaze MC
  myEvent.InitializeMC();

  bool eventRead=false;
  bool eventskip=false;
  while(!eventRead)
  {
    // Read blockid
    MAint32 blockid=0;
    *xdrinput_ >> blockid;
    if (xdrinput_->eof()) return StatusCode::FAILURE;

    MAint32 ntot=0;
    *xdrinput_ >> ntot;

    std::string version;
    *xdrinput_ >> version;

    if      (blockid==EVENTTABLE )       DecodeEventTable (version);
    else if (blockid==EVENTHEADER)       DecodeEventHeader(version);
    else if (blockid==MCFIO_STDHEPBEG ||
             blockid==MCFIO_STDHEPEND)   {DecodeSTDCM1 (version,mySample); }
    else if (blockid==MCFIO_STDHEP)
    {
      eventskip = !DecodeEventData(version, myEvent);
      eventRead = true;
    }
    else if (blockid==MCFIO_STDHEP4)
    {
      eventskip = !DecodeSTDHEP4 (version, myEvent);
      eventRead = true;
    }
    else
    {
      ERROR << "Block with the ID=" << blockid 
            << " is not managed by SampleAnalyzer" << endmsg;
      return StatusCode::SKIP;
    }
  }

  // return
  if(eventskip) return StatusCode::SKIP;
  return StatusCode::KEEP;
}


// -----------------------------------------------------------------------------
// DecodeFileHeader
// -----------------------------------------------------------------------------
bool STDHEPreader::DecodeFileHeader(SampleFormat& mySample)
{
  // temporary variables used for reading the xdr format file
  std::string  tmps;
  MAint32  tmpi = 0;
  MAuint32 tmpui = 0;

  // BlockID
  *xdrinput_ >> tmpi;
  if (tmpi != FILEHEADER)
  {
    ERROR << "header block not found" << endmsg;
    return false;
  }

  // Ntot
  *xdrinput_ >> tmpi;

  // STDHEP version
  *xdrinput_ >> tmps;
  SetVersion(tmps);
  if (version_==UNKNOWN) 
  {
    ERROR << "stdhep version unknown : '" << tmps << endmsg;
    return false;
  }

  // Title
  tmps="";
  *xdrinput_ >> tmps;
  //  std::cout << "title=" << tmps << std::cout;

  // Set the title in lower case
  tmps = CONVERT->ToLower(tmps);
  if (tmps.find("pythia")!=std::string::npos)
  {
    mySample.SetSampleGenerator(MA5GEN::PYTHIA6);
  }
  else if (tmps.find("herwig")!=std::string::npos)
  {
    mySample.SetSampleGenerator(MA5GEN::HERWIG6);
  }
  else
  {
    mySample.SetSampleGenerator(MA5GEN::UNKNOWN);
  }

  //std::cout << "title=" << tmps << std::endl;

  // Comment
  *xdrinput_ >> tmps;
  //std::cout << "comment=" << tmps << std::endl; 

  // Creation date
  *xdrinput_ >> tmps;
  //std::cout << "date=" << tmps << endmsg;

  // Closing date (only in version 2.01)
  if (version_==V21)  
  {
    *xdrinput_ >> tmps;
    //std::cout << "cdate=" << tmps << endmsg;
  }

  // Expected number of events
  *xdrinput_ >> tmpui;
  //std::cout << "Nevents = " << tmpui << endmsg;

  // Number of events
  *xdrinput_ >> tmpui;
  //std::cout << "Nevents = " << tmpui << endmsg;

  // First table
  *xdrinput_ >> tmpui;
  //std::cout << "First table=" << tmpui << endmsg;

  // Dim Table
  MAuint32 dimTable;
  *xdrinput_ >> dimTable;
  //std::cout << "Dim table=" << dimTable << endmsg;

  // Number of blocks
  MAuint32 nBlocks;
  *xdrinput_ >> nBlocks;
  //std::cout << "N blocks = " << nBlocks << endmsg;

  // Number of NTuples
  MAuint32 nNTuples = 0;
  if (version_!=V1)
  {
    *xdrinput_ >> nNTuples;
    //std::cout << "Nb NTuples = " << nNTuples << endmsg;
  }

  // Processing blocks extraction 
  if (nBlocks!=0)
  {
    // Extracting blocks
    std::vector<MAint32> blocks;
    *xdrinput_ >> blocks;  
 
    // Extracting block names
    for (MAuint32 i=0;i<blocks.size();i++)
    {
      *xdrinput_ >> tmps;
      //std::cout << "Block " << i << " = " << tmps; 
    }
  }

  // Processing ntuple extraction (only in version 2)
  /*if (version_!=V1 && nNTuples!=0)
  {
    // Loop over ntuple
    for (unsigned int i=0;i<nNTuples;i++)
    {
      // Number of characters in title
      if ( !xdr_int(&xdr_, &tmpi) ) return false;
      unsigned int nctitle = tmpi;

      // Number of characters in category
      if ( !xdr_int(&xdr_, &tmpi) ) return false;
      unsigned int nccategory = tmpi;

      // IdRef
      if ( !xdr_int(&xdr_, &tmpi) ) return false;

      // uid
      if ( !xdr_int(&xdr_, &tmpi) ) return false;

      // Title of the Ntuple
      if ( !xdr_string(&xdr_, &tmps,nctitle) ) return false;

      // Category of the Ntuple
      if ( !xdr_string(&xdr_, &tmps,nccategory) ) return false;

      INFO << "ntu ... to finish" << endmsg;

    }
  }
  */

  return true;
}


// -----------------------------------------------------------------------------
// FinalizeHeader
// -----------------------------------------------------------------------------
bool STDHEPreader::FinalizeHeader(SampleFormat& mySample)
{


  return true;
}


// -----------------------------------------------------------------------------
// DecodeEventTable
// -----------------------------------------------------------------------------
bool STDHEPreader::DecodeEventTable(const std::string& evt_version)
{
  // Decoding the event
  // Case: classical 
  if (evt_version=="1.00")
  {
    MAint32 idat=0;
    *xdrinput_ >> idat;

    MAuint32 uidat=0;
    *xdrinput_ >> uidat; 

    // Extracting evtnums
    std::vector<MAint32> evtnums;
    *xdrinput_ >> evtnums;

    // Extracting storenums
    std::vector<MAint32> storenums;
    *xdrinput_ >> storenums;

    // Extracting runnums
    std::vector<MAint32> runnums;
    *xdrinput_ >> runnums;

    // Extracting trigMasks
    std::vector<MAuint32> NtrigMasks;
    *xdrinput_ >> NtrigMasks;

    // Extracting prtEvents
    std::vector<MAuint32> NptrEvents;
    *xdrinput_ >> NptrEvents;
  }

  // Pavel's adding: 64-bit adress
  else if (evt_version=="2.00")
  {
    MAint32 idat=0;
    *xdrinput_ >> idat;

    MAuint64 uidat=0;
    *xdrinput_ >> uidat; 

    // Extracting evtnums
    std::vector<MAint32> evtnums;
    *xdrinput_ >> evtnums;

    // Extracting storenums
    std::vector<MAint32> storenums;
    *xdrinput_ >> storenums;

    // Extracting runnums
    std::vector<MAint32> runnums;
    *xdrinput_ >> runnums;

    // Extracting trigMasks
    std::vector<MAuint32> NtrigMasks;
    *xdrinput_ >> NtrigMasks;

    // Extracting prtEvents
    std::vector<MAuint64> NptrEvents;
    *xdrinput_ >> NptrEvents;
  }

  // Case: other version?
  else 
  {
    ERROR << "version '" << evt_version << "' is not supported" << endmsg;
    return false;
  }

  return true;
}


// -----------------------------------------------------------------------------
// DecodeEventHeader
// -----------------------------------------------------------------------------
bool STDHEPreader::DecodeEventHeader(const std::string& evt_version)
{
  MAint32 evtnum=0;
  *xdrinput_ >> evtnum;

  MAint32 storenums=0;
  *xdrinput_ >> storenums;

  MAint32 runnum=0;
  *xdrinput_ >> runnum;

  MAint32 trigMask=0;
  *xdrinput_ >> trigMask;

  MAuint32 nBlocks=0;
  *xdrinput_ >> nBlocks;

  MAuint32 dimBlocks=0;
  *xdrinput_ >> dimBlocks;

  MAuint32 nNTuples=0;
  MAuint32 dimNTuples=0;

  // Is there NTuple
  bool skipNTuples = false;
  bool add64bit=false;
  if (evt_version=="2.00" || evt_version=="3.00") skipNTuples=true;
  if (evt_version=="3.00") add64bit=true;

  // NTuple
  if (skipNTuples)
  {
    *xdrinput_ >> nNTuples;
    *xdrinput_ >> dimNTuples;
  }

  // Processing blocks extraction 
  if (dimBlocks>0)
  {
    // Extracting blocks
    std::vector<MAint32> blocks;
    *xdrinput_ >> blocks;

    // Extracting blocks
    if (!add64bit)
    {
      std::vector<MAuint32> ptrBlocks;
      *xdrinput_ >> ptrBlocks;
    }
    else
    {
      std::vector<MAuint64> ptrBlocks;
      *xdrinput_ >> ptrBlocks;
    }
  }

  // Processing blocks extraction 
  if (skipNTuples && dimNTuples>0)
  {
    // Extracting blocks
    std::vector<MAint32> nTupleIds;
    *xdrinput_ >> nTupleIds;

    // Extracting blocks
    if (!add64bit)
    {
      std::vector<MAuint32> ptrNTuples;
      *xdrinput_ >> ptrNTuples;
    }
    else
    {
      std::vector<MAuint64> ptrNTuples;
      *xdrinput_ >> ptrNTuples;
    }
  }

  return true;
}


// -----------------------------------------------------------------------------
// DecodeSTDCM1
// -----------------------------------------------------------------------------
bool STDHEPreader::DecodeSTDCM1(const std::string& version, SampleFormat& mySample)
{
  MAint32 nevtreq; 
  *xdrinput_ >> nevtreq;

  MAint32 nevtgen; 
  *xdrinput_ >> nevtgen;

  MAint32 nevtwrt;
  *xdrinput_ >> nevtwrt;

  MAfloat32 stdecom;
  *xdrinput_ >> stdecom; 

  MAfloat32 stdxsec;
  *xdrinput_ >> stdxsec;
  if (mySample.mc()!=0)
  {
    mySample.mc()->setXsectionMean(stdxsec);
    mySample.mc()->setXsectionError(0);
  }

  MAfloat64 stdseed1; 
  *xdrinput_ >> stdseed1;

  MAfloat64 stdseed2;
  *xdrinput_ >> stdseed2;

  if (version.find("1.")==0 || version.find("2.")==0 || 
      version.find("3.")==0 || version.find("4.")==0 || 
      version.find("5.00")==0) return true;

  std::string tmps;
  *xdrinput_ >> tmps;
  *xdrinput_ >> tmps;

  if (version.find("5.00")==0 || version.find("5.01")==0 )
   return true;

  MAint32 nevtlh=0;
  *xdrinput_ >> nevtlh;

  return true;

}


// -----------------------------------------------------------------------------
// DecodeEventFormat
// -----------------------------------------------------------------------------
bool STDHEPreader::DecodeEventData(const std::string& version, 
                                   EventFormat& myEvent)
{
  Reset();

  // Extracting the event number
  *xdrinput_ >> nevhept_;  

  // Extracting the number of particles
  *xdrinput_ >> nhept_;

  // Extracting isthept
  *xdrinput_ >> isthept_;

  // Extracting idhept
  *xdrinput_ >> idhept_;

  // Extracting jmohept
  *xdrinput_ >>  jmohept_;

  // Extracting jdahept
  *xdrinput_ >> jdahept_;

  // Extracting 
  *xdrinput_ >> phept_;

  // Extracting 
  *xdrinput_ >> vhept_;

  // Check the consistency of the event
  if(!CheckEvent(myEvent,"STDHEP")) return false;

  // Loop over particles
  for (unsigned int i=0;i<static_cast<unsigned int>(nhept_);i++)
  {
    // Get a new particle
    MCParticleFormat * part = myEvent.mc()->GetNewParticle();

    // Fill the data format
    part->pdgid_=idhept_[i];
    part->statuscode_=isthept_[i];
    part->mothup1_=jmohept_[2*i];
    part->mothup2_=jmohept_[2*i+1];
    part->daughter1_=jdahept_[2*i];
    part->daughter2_=jdahept_[2*i+1];
    part->momentum_.SetPx(phept_[5*i]);
    part->momentum_.SetPy(phept_[5*i+1]);
    part->momentum_.SetPz(phept_[5*i+2]);
    part->momentum_.SetE (phept_[5*i+3]);
  }

  return true;
}

bool STDHEPreader::CheckEvent(const EventFormat& myEvent, 
                              const std::string& blk)
{
  if(nhept_<0) 
  {
     ERROR << "Corrupted " << blk << " block: negative number of particles."
           << " Event ignored ." << endmsg;
     return false;
  }
  if(nhept_!=static_cast<MAint32>(isthept_.size())) 
  {
     ERROR << "Corrupted " << blk << " block: missing status codes."
           << " Event ignored." << endmsg;
     return false;
  }
  if(nhept_!=static_cast<MAint32>(idhept_.size())) 
  {
     ERROR << "Corrupted " << blk << " block: missing PDG codes."
           << " Event ignored." << endmsg;
     return false;
  } 
  if((2*nhept_)!=static_cast<MAint32>(jmohept_.size())) 
  {
     ERROR << "Corrupted " << blk << " block: missing mother information."
           << " Event ignored." << endmsg;
     return false;
  } 
  if((2*nhept_)!=static_cast<MAint32>(jdahept_.size())) 
  {
     ERROR << "Corrupted " << blk << " block: missing daughter information."
           << " Event ignored." << endmsg;
     return false;
  } 
  if((5*nhept_)!=static_cast<MAint32>(phept_.size()))
  {
     ERROR << "Corrupted " << blk << " block: missing 4-momentum " << 
               "information." << " Event ignored." << endmsg;
     return false;
  }   
  if((4*nhept_)!=static_cast<MAint32>(vhept_.size()))
  {
     ERROR << "Corrupted " << blk << " block: missing vertex information."
           << " Event ignored." << endmsg;
     return false;
  }
  return true;
}

bool STDHEPreader::DecodeSTDHEP4(const std::string& version, 
                                   EventFormat& myEvent)
{
  Reset();

  // Extracting the event number
  *xdrinput_ >> nevhept_;  

  // Extracting the number of particles
  *xdrinput_ >> nhept_;

  // Extracting isthept
  *xdrinput_ >> isthept_;

  // Extracting idhept
  *xdrinput_ >> idhept_;

  // Extracting jmohept
  *xdrinput_ >>  jmohept_;

  // Extracting jdahept
  *xdrinput_ >> jdahept_;

  // Extracting 
  *xdrinput_ >> phept_;

  // Extracting 
  *xdrinput_ >> vhept_;

  // Extracting the event weight
  MAfloat64 eventweight=1;
  *xdrinput_ >> eventweight;
  myEvent.mc()->setWeight(eventweight);

  // Extracting alpha QED
  MAfloat64 alphaQED=0;
  *xdrinput_ >> alphaQED;

  // Extracting alpha QCD
  MAfloat64 alphaQCD=0;
  *xdrinput_ >> alphaQCD;

  // Extracing dat
  std::vector<MAfloat64> dat;
  *xdrinput_ >> dat;

  // Extracing dat
  std::vector<MAfloat64> spint;
  *xdrinput_ >> spint;

  // Extracting idat
  std::vector<MAint32> idat;
  *xdrinput_ >> idat;

  // Extracting idrupt
  MAint32 idrupt;
  *xdrinput_ >> idrupt;

  // Check the consistency of the entries in the event table
  if(!CheckEvent(myEvent, "STDHEP4")) return false;

  // Loop over particles
  for (unsigned int i=0;i<static_cast<unsigned int>(nhept_);i++)
  {
    // Get a new particle
    MCParticleFormat * part = myEvent.mc()->GetNewParticle();

    // Fill the data format
    part->pdgid_=idhept_[i];
    part->statuscode_=isthept_[i];
    part->mothup1_=jmohept_[2*i];
    part->mothup2_=jmohept_[2*i+1];
    part->daughter1_=jdahept_[2*i];
    part->daughter2_=jdahept_[2*i+1];
    part->momentum_.SetPx(phept_[5*i]);
    part->momentum_.SetPy(phept_[5*i+1]);
    part->momentum_.SetPz(phept_[5*i+2]);
    part->momentum_.SetE (phept_[5*i+3]);
  }

  return true;
}


// -----------------------------------------------------------------------------
// FinalizeEvent
// -----------------------------------------------------------------------------
bool STDHEPreader::FinalizeEvent(SampleFormat& mySample, EventFormat& myEvent)
{
  // Is it a bugged event ?
  if (!firstevent && nevhept_ == nevhept_before_) return false;  
  nevhept_before_ = nevhept_;
  firstevent=false;

  // Mother pointer assignment
  for (unsigned int i=0; i<myEvent.mc()->particles_.size();i++)
  {
    MCParticleFormat& part = myEvent.mc()->particles_[i];

    // MET, MHT, TET, THT
    if (part.statuscode()==1 && !PHYSICS->Id->IsInvisible(part))
    {
      myEvent.mc()->MET_ -= part.momentum();
      myEvent.mc()->TET_ += part.pt();
      if (PHYSICS->Id->IsHadronic(part))
      {
        myEvent.mc()->MHT_ -= part.momentum();
        myEvent.mc()->THT_ += part.pt(); 
      }
    }

    unsigned int index1=myEvent.mc()->particles_[i].mothup1_;
    unsigned int index2=myEvent.mc()->particles_[i].mothup2_;
    if (index1!=0 && index2==0) index2=index1;
    if (index1!=0)
    {
      try
      {
        if (index1>=myEvent.mc()->particles_.size()) throw EXCEPTION_WARNING("mother index1 is greater to nb of particles","",0);
        myEvent.mc()->particles_[i].mother1_ = &myEvent.mc()->particles_[index1-1];
        myEvent.mc()->particles_[index1-1].daughters_.push_back(&myEvent.mc()->particles_[i]);
      }
      catch (const std::exception& e)
      {
        MANAGE_EXCEPTION(e);
        //     << " - index1 = " << index1 << endmsg
        //     << " - particles.size() " << myEvent.mc()->particles_.size()
        myEvent.mc()->particles_[i].mother1_ = 0;
      }    
    }
    if (index2!=0)
    {
      try
      {
        if (index2>=myEvent.mc()->particles_.size()) throw EXCEPTION_WARNING("mother index2 is greater to nb of particles","",0);
        myEvent.mc()->particles_[i].mother2_ = &myEvent.mc()->particles_[index2-1];
        myEvent.mc()->particles_[index2-1].daughters_.push_back(&myEvent.mc()->particles_[i]);
      }
      catch (const std::exception& e)
      {
        MANAGE_EXCEPTION(e);
        //     << " - index1 = " << index1 << endmsg
        //     << " - particles.size() " << myEvent.mc()->particles_.size()
        myEvent.mc()->particles_[i].mother2_ = 0;
      }    
    }
  }

  // Finalize event
  myEvent.mc()->MET_.momentum().SetPz(0.);
  myEvent.mc()->MET_.momentum().SetE(myEvent.mc()->MET_.momentum().Pt());
  myEvent.mc()->MHT_.momentum().SetPz(0.);
  myEvent.mc()->MHT_.momentum().SetE(myEvent.mc()->MHT_.momentum().Pt());

  // Normal end
  return true;
}


// -----------------------------------------------------------------------------
// Finalize
// -----------------------------------------------------------------------------
bool STDHEPreader::Finalize()
{
  if (!ReaderTextBase::Finalize()) return false;
  if (xdrinput_!=0) delete xdrinput_;
  return true;  
}


// -----------------------------------------------------------------------------
// SetVersion
// -----------------------------------------------------------------------------
void STDHEPreader::SetVersion(const std::string& version)
{
  if (version.size()<2)       version_=UNKNOWN;
  else if (version[0]==1)     version_=V1;
  else if (version=="2.01")   version_=V21;
  else if (version[0]==2)     version_=V2;
  else version_=UNKNOWN; 
}
