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
    struct QueueItem item(&pitEntry);

    std::cout << "***Interest name: " << interest.getName() << std::endl;

    std::string s = interest.getName().getSubName(2,1).toUri();
    uint32_t dscp_value = std::stoi(s.substr(1));
    item.wireEncode = interest.wireEncode();
    item.packetType = INTEREST;
    item.interface = &inFace;

    m_tx_queue.DoEnqueue(item, dscp_value);
    //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
    prioritySend();
}

void
QosStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
    struct QueueItem item(&pitEntry);

    std::cout << "***Nack name: " << nack.getInterest().getName() << " Reason: "<<nack.getReason()<< std::endl;

    std::string s = nack.getInterest().getName().getSubName(2,1).toUri();
    uint32_t dscp_value = std::stoi(s.substr (1));

    item.wireEncode = nack.getInterest().wireEncode();
    item.packetType = NACK;
    item.interface = &inFace;

    m_tx_queue.DoEnqueue(item, dscp_value);
    //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
    prioritySend();
}

void
QosStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                           const Face& inFace, const Data& data)
{
    struct QueueItem item(&pitEntry);

    NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName() <<
            " inFace=" << inFace.getId() << " data=" << data.getName());
    std::cout << "***Data name: " << data.getName() << std::endl;

    this->beforeSatisfyInterest(pitEntry, inFace, data);
    std::string s = data.getName().getSubName(2,1).toUri();
    uint32_t dscp_value = std::stoi(s.substr (1));

    item.wireEncode = data.wireEncode();
    item.packetType = DATA;
    item.interface = &inFace;

    m_tx_queue.DoEnqueue(item, dscp_value);
    //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
    prioritySend();
}

void
QosStrategy::prioritySend()
{
    Interest interest;
    Data data;
    lp::Nack nack;
    int TOKEN_REQUIRED = 5;

    std::cout << "\nSend, tokens = ..........." << tb.GetTokens() << std::endl;
    if(tb.GetTokens() >= TOKEN_REQUIRED)
    {
        //determine howmany token is required
        double consumed = tb.ConsumeTokens(TOKEN_REQUIRED);
        std::cout << "CONSUME in QoS strategy, tokens = ..........." << consumed << std::endl;

        //Dequeue the packet
        struct QueueItem item = m_tx_queue.DoDequeue();
        interest.wireDecode( item.wireEncode );
        const Interest interest1 = interest;
        const shared_ptr<pit::Entry>* PE = &(item.pitEntry);

        switch(item.packetType) {
            case INTEREST:
                interest.wireDecode( item.wireEncode );
                prioritySendInterest(*(PE), *(item.interface), interest);
                break;

            case DATA:
                data.wireDecode( item.wireEncode );
                prioritySendData(*(PE), *(item.interface), data);
                break;

            case NACK:
                nack = lp::Nack(interest1);
                prioritySendNack(*(PE), *(item.interface), nack);
                break;

            default:
                std::cout<<"prioritySend(Invalid Type)\n";
                break;
        }
    }
    else
    {
        //TODO: wait till token is available
        std::cout << "Token not available...!!!" << std::endl;
    }

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
