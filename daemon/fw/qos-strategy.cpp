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
#include "../../../apps/ConsumedTokens.cpp"
#include "TBucket.hpp"
#include "ns3/simulator.h"
#include "../../../helper/ndn-scenario-helper.hpp"


namespace nfd {
namespace fw {

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
  CT.m_tokens = 0;
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("QosStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "QosStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
  int node= ns3::NodeContainer::GetGlobal().Get(ns3::Simulator::GetContext())->GetId();

  CT.sender1[node] = &m_sender1;
  CT.sender2[node] = &m_sender2;
  CT.sender3[node] = &m_sender3;
  CT.hasSender[node] = true;
  CT.sender1[node]->send.connect(
    [this](){
      this->prioritySend();
    });
  CT.sender2[node]->send.connect(
    [this](){
      this->prioritySend();
    });
  CT.sender3[node]->send.connect(
    [this](){
      this->prioritySend();
    });
}

const Name&
QosStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/qos/%FD%03");
  return strategyName;
}

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
QosStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
    struct QueueItem item(&pitEntry);
    //std::cout << "***Interest name: " << interest.getName() << std::endl;
    std::string s = interest.getName().getSubName(2,1).toUri();
    uint32_t dscp_value;

    if(interest.getName().getSubName(1,1).toUri() == "/typeI" ) {
        dscp_value = 1;
    } else if (interest.getName().getSubName(1,1).toUri() == "/typeII" ) {
        dscp_value = 21;
    } else if (interest.getName().getSubName(1,1).toUri() == "/be" ) {
        dscp_value = 60;
    } else {
        dscp_value = std::stoi(s.substr(1));
    }

    item.wireEncode = interest.wireEncode();
    item.packetType = INTEREST;
    item.inface = &inFace;
    if(dscp_value != 60 ){
        //uint32_t f = inFace.getId();
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

            //this->sendInterest(pitEntry, outFace, interest);
            uint32_t f = outFace.getId();
            item.outface = &outFace;
            m_tx_queue[f].DoEnqueue(item, dscp_value);

            /*
            if (m_tx_queue[f].DoEnqueue(item, dscp_value)==false) {
                this->rejectPendingInterest(pitEntry);
                lp::NackHeader nackHeader;
                nackHeader.setReason(lp::NackReason::NO_ROUTE);

                this->sendNack(pitEntry, inFace, nackHeader);
            }
            */

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

        //m_tx_queue[f].DoEnqueue(item, dscp_value);
        //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
        RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);

        if (suppression == RetxSuppressionResult::SUPPRESS) {
            NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << " suppressed");
            return;
        }
    }
    else {
        RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);

        if (suppression == RetxSuppressionResult::SUPPRESS) {
            NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << " suppressed");
            return;
        }

        const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
        const fib::NextHopList& nexthops = fibEntry.getNextHops();
        auto it = nexthops.end();

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
            uint32_t f = outFace.getId();
            item.outface = &outFace;
            m_tx_queue[f].DoEnqueue(item, dscp_value);

            /*
            if (m_tx_queue[f].DoEnqueue(item, dscp_value) == false) {
                lp::NackHeader nackHeader;
                nackHeader.setReason(lp::NackReason::NO_ROUTE);
                this->sendNack(pitEntry, inFace, nackHeader);
                this->rejectPendingInterest(pitEntry);

                return;
            }
            */

            NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << " newPitEntry-to=" << outFace.getId());
            prioritySend();
            return;
        }

        // find an unused upstream with lowest cost except downstream
        it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
                return isNextHopEligible(inFace, interest, nexthop, pitEntry, true, time::steady_clock::now());
                });

        if (it != nexthops.end()) {
            Face& outFace = it->getFace();
            uint32_t f = outFace.getId();
            item.outface = &outFace;
            m_tx_queue[f].DoEnqueue(item, dscp_value);

            /*
            if (m_tx_queue[f].DoEnqueue(item, dscp_value) == false) {
                this->rejectPendingInterest(pitEntry);
                lp::NackHeader nackHeader;
                nackHeader.setReason(lp::NackReason::NO_ROUTE);

                this->sendNack(pitEntry, inFace, nackHeader);

            }
            */

            NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << " retransmit-unused-to=" << outFace.getId());

            prioritySend();
            return;
        }

        // find an eligible upstream that is used earliest
        it = findEligibleNextHopWithEarliestOutRecord(inFace, interest, nexthops, pitEntry);
        if (it == nexthops.end()) {
            NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " retransmitNoNextHop");
        }
        else {
            Face& outFace = it->getFace();
            uint32_t f = outFace.getId();
            item.outface = &outFace;
            m_tx_queue[f].DoEnqueue(item, dscp_value);

            /*
            if (m_tx_queue[f].DoEnqueue(item, dscp_value) == false) {
                this->rejectPendingInterest(pitEntry);
                lp::NackHeader nackHeader;
                nackHeader.setReason(lp::NackReason::NO_ROUTE);

                this->sendNack(pitEntry, inFace, nackHeader);
            }
            */

            NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << " retransmit-retry-to=" << outFace.getId());
            prioritySend();
            return;
        }
    }
    prioritySend();
}

void
QosStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
    return;
    struct QueueItem item(&pitEntry);

    //std::cout << "***Nack name: " << nack.getInterest().getName() << " Reason: "<<nack.getReason()<< std::endl;

    std::string s = nack.getInterest().getName().getSubName(2,1).toUri();
    uint32_t dscp_value;

    if (nack.getInterest().getName().getSubName(1,1).toUri() == "/typeI" )
        dscp_value = 1;
    else if (nack.getInterest().getName().getSubName(1,1).toUri() == "/typeII" )
        dscp_value = 21;
    else if (nack.getInterest().getName().getSubName(1,1).toUri() == "/be" )
        dscp_value = 60;
    else dscp_value = std::stoi(s.substr(1));

    item.wireEncode = nack.getInterest().wireEncode();
    item.packetType = NACK;
    item.inface = &inFace;
    uint32_t f = inFace.getId();
    this->processNack(inFace, nack, pitEntry);

    //m_tx_queue[f].DoEnqueue(item, dscp_value);
    //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
    //prioritySend();
}

void
QosStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                           const Face& inFace, const Data& data)
{
    struct QueueItem item(&pitEntry);
    NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName() <<
            " inFace=" << inFace.getId() << " data=" << data.getName());
    //std::cout << "***Data name: " << data.getName() << std::endl;

    this->beforeSatisfyInterest(pitEntry, inFace, data);
    std::string s = data.getName().getSubName(2,1).toUri();
    uint32_t dscp_value;

    if (data.getName().getSubName(1,1).toUri() == "/typeI" )
        dscp_value = 1;
    else if (data.getName().getSubName(1,1).toUri() == "/typeII" )
        dscp_value = 21;
    else if (data.getName().getSubName(1,1).toUri() == "/be" )
        dscp_value = 60;
    else dscp_value = std::stoi(s.substr(1));

    item.wireEncode = data.wireEncode();
    item.packetType = DATA;
    item.inface = &inFace;
    //uint32_t f = inFace.getId();
    std::set<Face*> pendingDownstreams;
    auto now = time::steady_clock::now();

    // remember pending downstreams
    for (const pit::InRecord& inRecord : pitEntry->getInRecords()) {
        if (inRecord.getExpiry() > now) {
            if (inRecord.getFace().getId() == inFace.getId() &&
                    inRecord.getFace().getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) {
                continue;
            }
            pendingDownstreams.insert(&inRecord.getFace());
        }
    }

    for (const Face* pendingDownstream : pendingDownstreams) {
        uint32_t f = (*pendingDownstream).getId();
        item.outface = pendingDownstream;
        m_tx_queue[f].DoEnqueue(item, dscp_value);

        /*
        if(m_tx_queue[f].DoEnqueue(item, dscp_value) == false){
            lp::NackHeader nackHeader;
            nackHeader.setReason(lp::NackReason::NO_ROUTE);

            this->sendNack(pitEntry, *pendingDownstream, nackHeader);
        }
        */

        //this->sendData(pitEntry, data, *pendingDownstream);
    }

    //m_tx_queue[f].DoEnqueue(item, dscp_value);
    //ns3::Simulator::Schedule(ns3::Seconds(0.5), &QosStrategy::prioritySend, this);
    prioritySend();
}

void
QosStrategy::prioritySend()
{
    ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get(ns3::Simulator::GetContext());
    //std::cout << "\n|||||" << node->GetId()<<"||||||";
    Interest interest;
    Data data;
    lp::Nack nack;
    double TOKEN_REQUIRED = 1;
    bool tokenwait = false;
    //std::cout << "\n|||||" << node->GetId()<<"||||||";
    std::unordered_map<uint32_t,NdnPriorityTxQueue >::iterator itt = m_tx_queue.begin();

    while (itt != m_tx_queue.end()) {
        //std::cout << "\nSend, tokens = ..........." << m_sender1.m_tokens[itt->first] /*(TB.m_tokens - CT.m_tokens)*/ << std::endl;
        //std::cout << "\nSend, tokens = ..........." << m_sender2.m_tokens[itt->first] /*(TB.m_tokens - CT.m_tokens)*/ << std::endl;
        //std::cout << "\nSend, tokens = ..........." << m_sender3.m_tokens[itt->first] /*(TB.m_tokens - CT.m_tokens)*/ << std::endl;

        while (!m_tx_queue[itt->first].IsEmpty() && !tokenwait) {
            double token1 = CT.sender1[node->GetId()]->m_capacity;
            double token2 = CT.sender2[node->GetId()]->m_capacity;
            double token3 = CT.sender3[node->GetId()]->m_capacity;

            if (CT.sender1[node->GetId()]->m_tokens.find(itt->first) != CT.sender1[node->GetId()]->m_tokens.end())
                token1 = CT.sender1[node->GetId()]->m_tokens[itt->first];
            if (CT.sender2[node->GetId()]->m_tokens.find(itt->first) != CT.sender2[node->GetId()]->m_tokens.end())
                token2 = CT.sender2[node->GetId()]->m_tokens[itt->first];
            if (CT.sender3[node->GetId()]->m_tokens.find(itt->first) != CT.sender3[node->GetId()]->m_tokens.end())
                token3 = CT.sender3[node->GetId()]->m_tokens[itt->first];

            int choice = m_tx_queue[itt->first].SelectQueueToSend(token1, token2, token3);

            if (choice != -1) {
                TokenBucket *sender;

                if (choice == 0) {
                    sender = CT.sender1[node->GetId()];
                } else if (choice == 1){ sender = CT.sender2[node->GetId()];
                } else {
                    sender = CT.sender3[node->GetId()];
                }

                //if((sender.m_tokens >= TOKEN_REQUIRED) /*|| (CT.sender2->m_tokens >= TOKEN_REQUIRED) || (CT.sender3->m_tokens >= TOKEN_REQUIRED)*/)
                //  {
                //determine howmany token is required
                //CT.m_tokens += TOKEN_REQUIRED;

                //TODO: token bucket selection
                sender->hasFaces = true;
                sender->consumeToken(TOKEN_REQUIRED, itt->first);
                sender->m_need[itt->first] = 0;
                //CT.m_need = 0;
                //std::cout << "CONSUME in QoS strategy, tokens = ..........." << TOKEN_REQUIRED << std::endl;

                //Dequeue the packet
                struct QueueItem item = m_tx_queue[itt->first].DoDequeue(choice);
                const shared_ptr<pit::Entry>* PE = &(item.pitEntry);

                switch (item.packetType) {
                    case INTEREST:
                        interest.wireDecode( item.wireEncode );
                        //std::cout<<interest.getName()<<"  "<<node->GetId()<<std::endl;
                        prioritySendInterest(*(PE), *(item.inface), interest, (*item.outface));
                        break;

                    case DATA:
                        //std::cout<<"prioritySend(DATA)\n";
                        data.wireDecode( item.wireEncode );
                        prioritySendData(*(PE), *(item.inface), data, (*item.outface));
                        break;

                    case NACK:
                        //std::cout<<"prioritySend(NACK)\n";
                        interest.wireDecode( item.wireEncode );
                        nack = lp::Nack(interest);
                        prioritySendNack(*(PE), *(item.inface), nack);
                        break;

                    default:
                        //std::cout<<"prioritySend(Invalid Type)\n";
                        break;
                }
            } else {

                //TODO: wait till token is available
                CT.sender1[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqHig();
                CT.sender2[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqMid();
                CT.sender3[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqLow();
                tokenwait = true;
                //std::cout << "Token not available...!!!" << std::endl;
            }
        }
        itt++;
    }
}

void
QosStrategy::prioritySendData(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Data& data, const Face& outFace)
{
    this->sendData(pitEntry, data, outFace);
}

void
QosStrategy::prioritySendNack(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const lp::Nack& nack)
{
    this->processNack(inFace, nack, pitEntry);
}

void
QosStrategy::prioritySendInterest(const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Interest& interest, const Face& outFace)
{
    /*
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
        */
        //Face& outface = outFace;

        this->sendInterest(pitEntry, *const_pointer_cast<Face>(outFace.shared_from_this()), interest);
        /*
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
        //this->sendNack(pitEntry, inFace, nackHeader);

        this->rejectPendingInterest(pitEntry);
    }
    */
}

} // namespace fw
} // namespace nfd
