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
    std::cout << "***Interest name: " << interest.getName() << std::endl;

    std::string s = interest.getName().toUri();
    //TODO: Remember to adjust dscp value when namespace is changed.
    uint32_t dscp_value = std::stoi(s.substr (13,2));

    if(dscp_value >= 1 && dscp_value <= 20)
    {
        std::cout << "Dscp value: " << dscp_value << " **** High priority" << std::endl;
        //TODO:Enqueue Interest

    } else if(dscp_value >= 21 && dscp_value <= 40)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Medium priority" << std::endl;
        //TODO:Enqueue Interest

    }else if(dscp_value >= 41 && dscp_value <= 64)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Low priority" << std::endl;
        //TODO:Enqueue Interest

    } else
    {
        std::cout << "Incorrect dscp value !! Enqueue failed !!!! ";
    }

    //TODO: Remove later
    prioritySendInterest(pitEntry, inFace, interest);
    //prioritySend();

}

void
QosStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
    std::cout << "***Nack name: " << nack.getInterest().getName() << std::endl;

    std::string s = nack.getInterest().getName().toUri();
    //TODO: Remember to adjust dscp value when namespace is changed.
    uint32_t dscp_value = std::stoi(s.substr (13,2));

    if(dscp_value >= 1 && dscp_value <= 20)
    {
        std::cout << "Dscp value: " << dscp_value << " **** High priority" << std::endl;
        //TODO:Enqueue Nack

    } else if(dscp_value >= 21 && dscp_value <= 40)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Medium priority" << std::endl;
        //TODO:Enqueue Nack

    }else if(dscp_value >= 41 && dscp_value <= 64)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Low priority" << std::endl;
        //TODO:Enqueue Nack

    } else
    {
        std::cout << "Incorrect dscp value !! Enqueue failed !!!! ";
    }

    //TODO: Remove later
    prioritySendNack(pitEntry, inFace, nack);
    //prioritySend();

}

void
QosStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                           const Face& inFace, const Data& data)
{
  NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName() <<
          " inFace=" << inFace.getId() << " data=" << data.getName());
  this->beforeSatisfyInterest(pitEntry, inFace, data);

  std::cout << "***Data name: " << data.getName() << std::endl;
  std::string s = data.getName().toUri();
  //TODO: Remember to adjust dscp value when namespace is changed.
  uint32_t dscp_value = std::stoi(s.substr (13,2));

  if(dscp_value >= 1 && dscp_value <= 20)
  {
      std::cout << "Dscp value: " << dscp_value << " **** High priority" << std::endl;
      //TODO:EnqueueData

  } else if(dscp_value >= 21 && dscp_value <= 40)
  {
      std::cout << "Dscp value: " << dscp_value << " **** Medium priority" << std::endl;
      //TODO:EnqueueData

  }else if(dscp_value >= 41 && dscp_value <= 64)
  {
      std::cout << "Dscp value: " << dscp_value << " **** Low priority" << std::endl;
      //TODO:EnqueueData

  } else
  {
      std::cout << "Incorrect dscp value !! Enqueue failed !!!! ";
  }

  //TODO: Remove later
  prioritySendData(pitEntry, inFace, data);
  //prioritySend();

}

void
prioritySend()
{
#if 0
    //TODO: Dequeue using WFQ
    switch(type)
    {
        case INTEREST:
            prioritySendInterest(pitEntry, inFace, interest);
            break;
        case DATA:
            prioritySendData(pitEntry, inFace, data);
            break;
        case NACK:
            prioritySendNack(pitEntry, inFace, nack);
            break;
    }
#endif
}

void
QosStrategy::prioritySendData(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Data& data)
{
  this->sendDataToAll(pitEntry, inFace, data);
}

void
QosStrategy::prioritySendNack(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const lp::Nack& nack)
{
  this->processNack(inFace, nack, pitEntry);
}

void
QosStrategy::prioritySendInterest(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Interest& interest)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  int nEligibleNextHops = 0;

  bool isSuppressed = false;

  std::cout << "\nGOT INTEREST in QoS strategy, tokens = ..........." << tb.GetTokens() << std::endl;
  double consumed = tb.ConsumeTokens(30.0);
  std::cout << "CONSUME in QoS strategy, tokens = ..........." << consumed << std::endl;

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

} // namespace fw
} // namespace nfd
