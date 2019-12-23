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

#include "qos-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"

namespace nfd {
namespace fw {

TokenBucket tb(83.0, 3.0);

//const Name QosStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/qos/%FD%01");
NFD_REGISTER_STRATEGY(QosStrategy);

NFD_LOG_INIT(QosStrategy);

const time::milliseconds QosStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds QosStrategy::RETX_SUPPRESSION_MAX(250);

QosStrategy::QosStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("QosStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "QosStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));

  // Create input and output queue for each link - NMSU
  for (ns3::NodeList::Iterator node = ns3::NodeList::Begin(); node != ns3::NodeList::End(); node++) {
    std::cout << "********* Inside QosStrategy constructor" << std::endl;
    ns3::Ptr<ns3::ndn::GlobalRouter> source = (*node)->GetObject<ns3::ndn::GlobalRouter>();

    if (source == 0) {
        std::cout << "\nError: GlobalRouter object is empty!!";
        //continue;
    }
    else{
        std::cout << "\n********* Node: " << (*node)->GetId();
        ns3::ndn::GlobalRouter::IncidencyList& graphEdges = source->GetIncidencies();

        for (const auto& graphEdge : graphEdges) {
            int link = get<2>(graphEdge)->GetObject<ns3::Node>()->GetId();
            std::cout << "\n\t********* Link: " << link << std::endl;
            //TODO: create input output queue
        }
    }
  }
}

const Name&
QosStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/qos/%FD%03");
  return strategyName;
}

void
QosStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  int nEligibleNextHops = 0;

  bool isSuppressed = false;

  std::cout << "\nGOT INTEREST in QoS strategy, tokens = ..........." << tb.GetTokens() << std::endl;
  double consumed = tb.ConsumeTokens(30.0);
  std::cout << "CONSUME in QoS strategy, tokens = ..........." << consumed << std::endl;

#if 0
  //Get all the nodes and associated links in the topology
  for (ns3::NodeList::Iterator node = ns3::NodeList::Begin(); node != ns3::NodeList::End(); node++) {

      ns3::Ptr<ns3::ndn::GlobalRouter> source = (*node)->GetObject<ns3::ndn::GlobalRouter>();

      if (source == 0) {
          std::cout << "\nError: GlobalRouter object is empty!!";
          //continue;
      }
      else{
          std::cout << "\nNode: " << (*node)->GetId();
          ns3::ndn::GlobalRouter::IncidencyList& graphEdges = source->GetIncidencies();

          for (const auto& graphEdge : graphEdges) {
              int link = get<2>(graphEdge)->GetObject<ns3::Node>()->GetId();
              std::cout << "\n\tLink: " << link << std::endl;
          }
      }
  }
#endif

  for (const auto& nexthop : nexthops) {
    Face& outFace = nexthop.getFace();

    RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream(*pitEntry, outFace);

    if (suppressResult == RetxSuppressionResult::SUPPRESS) {
      NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << "to=" << outFace.getId() << " suppressed");
      isSuppressed = true;
      continue;
    }

    if ((outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
        wouldViolateScope(inFace, interest, outFace)) {
      continue;
    }

    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " pitEntry-to=" << outFace.getId());

    if (suppressResult == RetxSuppressionResult::FORWARD) {
      m_retxSuppression.incrementIntervalForOutRecord(*pitEntry->getOutRecord(outFace));
    }
    ++nEligibleNextHops;
  }

  if (nEligibleNextHops == 0 && !isSuppressed) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " noNextHop");

    lp::NackHeader nackHeader;
    nackHeader.setReason(lp::NackReason::NO_ROUTE);
    this->sendNack(pitEntry, inFace, nackHeader);

    this->rejectPendingInterest(pitEntry);
  }
}

void
QosStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(inFace, nack, pitEntry);
}

} // namespace fw
} // namespace nfd
