/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "best-route-strategy-attacker.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"
#include "ns3/simulator.h"
#include "../../../helper/ndn-scenario-helper.hpp"
#include "../../../helper/ndn-region-helper.hpp"
#include <map>
#include <utility>
#include <algorithm>
#include <string>
#include "../../../apps/AttackerRef.cpp"
namespace nfd {
namespace fw {

NFD_LOG_INIT(BestRouteStrategyAttacker);
NFD_REGISTER_STRATEGY(BestRouteStrategyAttacker);

const time::milliseconds BestRouteStrategyAttacker::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds BestRouteStrategyAttacker::RETX_SUPPRESSION_MAX(250);

BestRouteStrategyAttacker::BestRouteStrategyAttacker(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("BestRouteStrategyAttacker does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "BestRouteStrategyAttacker does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
BestRouteStrategyAttacker::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/best-route-attacker/%FD%05");
  return strategyName;
}

/** \brief determines whether a NextHop is eligible
 *  \param inFace incoming face of current Interest
 *  \param interest incoming Interest
 *  \param nexthop next hop
 *  \param pitEntry PIT entry
 *  \param wantUnused if true, NextHop must not have unexpired out-record
 *  \param now time::steady_clock::now(), ignored if !wantUnused
 */
static bool
isNextHopEligible(const Face& inFace, const Interest& interest,
                  const fib::NextHop& nexthop,
                  const shared_ptr<pit::Entry>& pitEntry,
                  bool wantUnused = false,
                  time::steady_clock::TimePoint now = time::steady_clock::TimePoint::min())
{
  const Face& outFace = nexthop.getFace();

  // do not forward back to the same face, unless it is ad hoc
  if (outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC)
    return false;

  // forwarding would violate scope
  if (wouldViolateScope(inFace, interest, outFace))
    return false;

  if (wantUnused) {
    // nexthop must not have unexpired out-record
    auto outRecord = pitEntry->getOutRecord(outFace);
    if (outRecord != pitEntry->out_end() && outRecord->getExpiry() > now) {
      return false;
    }
  }

  return true;
}

/** \brief pick an eligible NextHop with earliest out-record
 *  \note It is assumed that every nexthop has an out-record.
 */
static fib::NextHopList::const_iterator
findEligibleNextHopWithEarliestOutRecord(const Face& inFace, const Interest& interest,
                                         const fib::NextHopList& nexthops,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  auto found = nexthops.end();
  auto earliestRenewed = time::steady_clock::TimePoint::max();

  for (auto it = nexthops.begin(); it != nexthops.end(); ++it) {
    if (!isNextHopEligible(inFace, interest, *it, pitEntry))
      continue;

    auto outRecord = pitEntry->getOutRecord(it->getFace());
    BOOST_ASSERT(outRecord != pitEntry->out_end());
    if (outRecord->getLastRenewed() < earliestRenewed) {
      found = it;
      earliestRenewed = outRecord->getLastRenewed();
    }
  }

  return found;
}

void
BestRouteStrategyAttacker::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  //find top interest
  //in each 5 s interval and print
	
  if(ns3::Simulator::Now().GetSeconds()<checktime){
     Name device = interest.getName().getSubName(3,1);
     std::string trafficType = interest.getName().getSubName(-2,1).toUri();

     std::string s =device.toUri();//example device name /BESS14 /SW2

     int tempVariable=0;
     if ((s.substr(0,3)=="/PV"||s.substr(0,5) =="/BESS") && trafficType.find("attack")==std::string::npos){//(trafficType != "//attack")
    	 //std::cout<<"TOP Traffic Used for counting "<<interest.getName().toUri()<<std::endl;
    	 //std::cout<<"TOP Traffic Used for counting traffic Type ="<<trafficType<<"-"<<std::endl;
	  int pld =interest.getPayloadLength();
	  if(nameMap.find(device) == nameMap.end()) {
  	     int c = 0;//nameMap[device].count;
	     c++;
	     tempVariable=c;
	     nameMap[device]=BestRouteStrategyAttacker::nameDetails(c,pld);
	     attacker_map.popMap[device] = 1;

	  }
	  else {
	    int c = nameMap[device].count;
	    attacker_map.popMap[device]++;
	    c++;
	    tempVariable=c;
	    nameMap[device]=BestRouteStrategyAttacker::nameDetails(c,pld);

	   }
	  //std::cout<<"TOP Device name added to pop_map "<<device<<" with freq "<<attacker_map.popMap[device]<<std::endl;
     }
  }
  else{
     std::cout<<"Strategy test. simulator time: "<<ns3::Simulator::Now().GetSeconds()<<" checktime: "<<checktime<<std::endl;
     //clear map and reset checktime
     std::pair<Name, BestRouteStrategyAttacker::nameDetails> entryWithMaxValue = std::make_pair("0", BestRouteStrategyAttacker::nameDetails(0,0));

     for (auto currentEntry = nameMap.begin(); currentEntry != nameMap.end(); ++currentEntry) {
        if (currentEntry->second.count > entryWithMaxValue.second.count) {
 	   entryWithMaxValue = make_pair(
		                 currentEntry->first,
                		 nameDetails(currentEntry->second.count,currentEntry->second.payload));
	}

        //print the top
        //std::cout<<"TOP Entries "<<currentEntry->first<<" count: "<<currentEntry->second.count<<std::endl;
     }

     if(entryWithMaxValue.second.count!=0 &&  entryWithMaxValue.first!="\\0"&& ns3::Simulator::GetContext()<=3600){
        //std::cout<<"Max value for " << entryWithMaxValue.first << ": " << entryWithMaxValue.second.count << "\n";
        //std::cout<<"Sync context "<<ns3::Simulator::GetContext()<<std::endl;
        int node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() )->GetId();
        //std::cout<<"TOP Possible Lead Node : "<<node<<std::endl;
        attacker_map.targetMap[node]=entryWithMaxValue.first;
        attacker_map.setMap[node] = true;

     }

     checktime=(int(ns3::Simulator::Now().GetSeconds())/10)*10;
     checktime+=10;
     if (attacker_map.lastClear <= checktime){
	     //attacker_map.popMap.clear();
	     attacker_map.lastClear=checktime;
     }
     //nameMap.clear();
  }

  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();


  //int node= ns3::NodeContainer::GetGlobal().Get(ns3::Simulator::GetContext())->GetId();
  if (suppression == RetxSuppressionResult::NEW) {
    // forward to nexthop with lowest cost except downstream
    it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
      return isNextHopEligible(inFace, interest, nexthop, pitEntry);
    });

    if (it == nexthops.end()) {
      NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " noNextHop");

      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(pitEntry, inFace, nackHeader);

      this->rejectPendingInterest(pitEntry);
      return;
    }

    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " newPitEntry-to=" << outFace.getId());
    return;
  }

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
    return isNextHopEligible(inFace, interest, nexthop, pitEntry, true, time::steady_clock::now());
  });

  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " retransmit-unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(inFace, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " retransmitNoNextHop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " retransmit-retry-to=" << outFace.getId());
  }
}

void
BestRouteStrategyAttacker::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                     const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(inFace, nack, pitEntry);
}

} // namespace fw
} // namespace nfd
