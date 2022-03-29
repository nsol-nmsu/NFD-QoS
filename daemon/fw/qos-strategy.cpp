/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright ( C ) 2020 New Mexico State University- Board of Regents
 *
 * George Torres, Anju Kunnumpurathu James
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * ( at your option ) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qos-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"
#include "../../../apps/TBucketRef.hpp"
#include "TBucketDebug.hpp"
#include "ns3/simulator.h"
#include "../../../helper/ndn-scenario-helper.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include "model/ndn-net-device-transport.hpp"
#include "ns3/ptr.h"
#include "ns3/net-device.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/queue.h"


namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY( QosStrategy );

NFD_LOG_INIT( QosStrategy );

const time::milliseconds QosStrategy::RETX_SUPPRESSION_INITIAL( 10 );
const time::milliseconds QosStrategy::RETX_SUPPRESSION_MAX( 250 );

QosStrategy::QosStrategy( Forwarder& forwarder, const Name& name )
  : Strategy( forwarder )
  , ProcessNackTraits( this )
  , m_retxSuppression( RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX )
{
  //CT.m_tokens = 0;
  ParsedInstanceName parsed = parseInstanceName( name );

  if( !parsed.parameters.empty() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( "QosStrategy does not accept parameters" ) );
  }

  if( parsed.version && *parsed.version != getStrategyName()[-1].toVersion() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( 
          "QosStrategy does not support version " + to_string( *parsed.version ) ) );
  }

  this->setInstanceName( makeInstanceName( name, getStrategyName() ) );
  forwarder.beforeExpirePendingInterest.connect([this](const pit::Entry& entry){
       this->beforeExpirePendingInterest(entry);
  });
  setSuccessReqs({1.0, 1.0, 0.4});
}

void
QosStrategy::setUp(){
  int node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() )->GetId();
  if(!CT.drivers[node]->isSet()){
     CT.drivers[node]->setSize(3);
     CT.drivers[node]->addTokenBucket(&m_sender1);
     CT.drivers[node]->addTokenBucket(&m_sender2);
     CT.drivers[node]->addTokenBucket(&m_sender3);
     m_sender1.send.connect( [this]() {
        this->prioritySend();
       } );
     m_sender2.send.connect( [this]() {
        this->prioritySend();
       } );
     m_sender3.send.connect( [this]() {
        this->prioritySend();
        } );
  }
}

const Name&
QosStrategy::getStrategyName()
{
  static Name strategyName( "/localhost/nfd/strategy/qos/%FD%03" );
  return strategyName;
}

static bool
isNextHopEligible( const Face& inFace, const Interest& interest,
    const fib::NextHop& nexthop,
    const shared_ptr<pit::Entry>& pitEntry,
    bool wantUnused = false,
    time::steady_clock::TimePoint now = time::steady_clock::TimePoint::min(),
    uint32_t limit = 100 )
{
  const Face& outFace = nexthop.getFace();

  // Do not forward back to the same face, unless it is ad hoc.
  if( outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC )
    return false;

  // Forwarding would violate scope.
  if( wouldViolateScope( inFace, interest, outFace ) )
    return false;

  if( wantUnused ) {
    // Nexthop must not have unexpired out-record
    auto outRecord = pitEntry->getOutRecord( outFace );
    if( outRecord != pitEntry->out_end() && outRecord->getExpiry() > now ) {
      return false;
    }
  }
  ns3::ndn::NetDeviceTransport* device = dynamic_cast<ns3::ndn::NetDeviceTransport*>( outFace.getTransport());
  if(device == NULL){
     return false;
  }
  uint32_t rate = ns3::DynamicCast<ns3::QueueBase>(ns3::DynamicCast<ns3::PointToPointNetDevice>( device->GetNetDevice())->GetQueue())->GetNPackets() ;
  if(rate > limit)
         return false;

  return true;
}

/** \brief Pick an eligible NextHop with earliest out-record.
 *  \note It is assumed that every nexthop has an out-record.
 */
static fib::NextHopList::const_iterator
findEligibleNextHopWithEarliestOutRecord( const Face& inFace, const Interest& interest,
    const fib::NextHopList& nexthops,
    const shared_ptr<pit::Entry>& pitEntry )
{
  auto found = nexthops.end();
  auto earliestRenewed = time::steady_clock::TimePoint::max();

  for( auto it = nexthops.begin(); it != nexthops.end(); ++it ) {
    if( !isNextHopEligible( inFace, interest, *it, pitEntry ) )
      continue;

    auto outRecord = pitEntry->getOutRecord( it->getFace() );
    BOOST_ASSERT( outRecord != pitEntry->out_end() );

    if( outRecord->getLastRenewed() < earliestRenewed ) {
      found = it;
      earliestRenewed = outRecord->getLastRenewed();
    }
  }

  return found;
}

int
QosStrategy::getPrType(Name pkt){
   if( pkt.getSubName( 1,1 ).toUri() == "/typeI"  ) {
      return 0;
  } else if( pkt.getSubName( 1,1 ).toUri() == "/typeII"  ) {
       return 1;
  } else if( pkt.getSubName( 1,1 ).toUri() == "/typeIII"  ) {
       return 2;
  } 
  return 715;

}

void
QosStrategy::afterReceiveInterest( const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry )
{
  struct QueueItem item( &pitEntry );
  //std::string s = interest.getName().getSubName( 2,1 ).toUri();
  uint32_t pr_level = getPrType(interest.getName());

  item.wireEncode = interest.wireEncode();
  item.packetType = INTEREST;
  item.inface = &inFace;

  bool  forwarded = false;
  if( pr_level < m_successReqs.size()  ) {
    double successProb = m_successReqs[pr_level];
    //get name prefix
    std::string namePrefix = interest.getName().getSubName( 0, interest.getName().size()-1).toUri();
    //std::cout<<namePrefix<<std::endl;

    //check if initilized 
    if(m_faceStats.find(namePrefix) == m_faceStats.end()){
       initialize(namePrefix, pitEntry);
    }

    //do bootstrap test
    m_faceStats[namePrefix].totalPacketsSent++;
    if(!m_faceStats[namePrefix].bootstrapped && m_faceStats[namePrefix].totalPacketsSent > 3){
       m_faceStats[namePrefix].bootstrapped = true;
    }
   
    const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
    const fib::NextHopList& nexthops = fibEntry.getNextHops();
    int nEligibleNextHops = 0;
    bool isSuppressed = false;

    double prob = 0;
    bool hasOptions = true;
    bool first = true;
    unordered_map<uint32_t, int> seenFaces;
    while (first || (prob < successProb && hasOptions)){
       if (first) first = false;
       uint32_t bestFaceID = 0;
       double  bestProb = 0;
       const Face* bestFace = 0;

       for( const auto& nexthop : nexthops ) {
          Face& outFace = nexthop.getFace();
          RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream( *pitEntry, outFace );

         if( suppressResult == RetxSuppressionResult::SUPPRESS ) {
            NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
               << "to=" << outFace.getId() << " suppressed" );
           isSuppressed = true;
           continue;
         }

         if( ( outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) ||
            wouldViolateScope( inFace, interest, outFace ) ) {
            continue;
         }
         uint32_t f = outFace.getId();
         if(seenFaces[f] == 1){
            continue;
         }
      

         double faceProb = 0;
         //getProb
         if(bestFaceID==0)
            faceProb = getFaceProb(namePrefix, f, false);
         else 
            faceProb = getFaceProb(namePrefix, f, true);

         if(faceProb >= bestProb){
            bestProb = faceProb;
            bestFaceID = f;
            bestFace = &outFace;
         }
         ++nEligibleNextHops;
       }

       if(bestFaceID == 0){
          hasOptions = false;
          continue;
       }
       item.outface = bestFace;
       if (m_tx_queue.find(bestFaceID) == m_tx_queue.end()){
          int size = m_successReqs.size();	       
          m_tx_queue[bestFaceID].initialize(size);
       }
       if(m_tx_queue[bestFaceID].DoEnqueue( item, pr_level ))
	       forwarded = true;
       prob += bestProb;
       seenFaces[bestFaceID] = 1;

       //NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
       //    << " pitEntry-to=" << outFace.getId() );

       //if( suppressResult == RetxSuppressionResult::FORWARD ) {
       //   m_retxSuppression.incrementIntervalForOutRecord( *pitEntry->getOutRecord( bestFace ) );
       //}

    }
    prioritySend();

    if( nEligibleNextHops == 0 && !isSuppressed ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " noNextHop" );

      lp::NackHeader nackHeader;

      nackHeader.setReason( lp::NackReason::NO_ROUTE );

      this->sendNack( pitEntry, inFace, nackHeader );
      this->rejectPendingInterest( pitEntry );
    }

    RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry( *pitEntry );

    if( suppression == RetxSuppressionResult::SUPPRESS ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " suppressed" );
      return;
    }
  } else {
    RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry( *pitEntry );

    if( suppression == RetxSuppressionResult::SUPPRESS ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " suppressed" );
      return;
    }

    const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
    const fib::NextHopList& nexthops = fibEntry.getNextHops();
    auto it = nexthops.end();

    if( suppression == RetxSuppressionResult::NEW ) {
      // Forward to nexthop with lowest cost except downstream.
      it = std::find_if( nexthops.begin(), nexthops.end(), [&] ( const auto& nexthop ) {
          return isNextHopEligible( inFace, interest, nexthop, pitEntry );
          } );

      if( it == nexthops.end() ) {
        NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " noNextHop" );

        lp::NackHeader nackHeader;
        nackHeader.setReason( lp::NackReason::NO_ROUTE );
        this->sendNack( pitEntry, inFace, nackHeader );

        this->rejectPendingInterest( pitEntry );
        return;
      }

      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
       if (m_tx_queue.find(f) == m_tx_queue.end()){
	  int size = m_successReqs.size();
          m_tx_queue[f].initialize(size);
       }
      
      if(m_tx_queue[f].DoEnqueue( item, pr_level ))
         forwarded = true;

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " newPitEntry-to=" << outFace.getId() );
      prioritySend();
      return;
    }

    // Find an unused upstream with lowest cost except downstream.
    it = std::find_if( nexthops.begin(), nexthops.end(), [&] ( const auto& nexthop ) {
        return isNextHopEligible( inFace, interest, nexthop, pitEntry, true, time::steady_clock::now() );
        } );

    if( it != nexthops.end() ) {
      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
       if (m_tx_queue.find(f) == m_tx_queue.end()){
	  int size = m_successReqs.size();
          m_tx_queue[f].initialize(size);
       }
      if(m_tx_queue[f].DoEnqueue( item, pr_level ))
         forwarded = true;

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " retransmit-unused-to=" << outFace.getId() );

      prioritySend();
      return;
    }

    // Find an eligible upstream that is used earliest.
    it = findEligibleNextHopWithEarliestOutRecord( inFace, interest, nexthops, pitEntry );
    if( it == nexthops.end() ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " retransmitNoNextHop" );
    } else {
      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
       if (m_tx_queue.find(f) == m_tx_queue.end()){
	  int size = m_successReqs.size();
          m_tx_queue[f].initialize(size);
       }

      if(m_tx_queue[f].DoEnqueue( item, pr_level ))
         forwarded = true;

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " retransmit-retry-to=" << outFace.getId() );
      prioritySend();
      return;
    }
  }


  if (!forwarded) {
     this->rejectPendingInterest( pitEntry );
     rejectedInterests.push_back(interest.getName());
     return;
  }
  prioritySend();
}

void
QosStrategy::afterReceiveNack( const Face& inFace, const lp::Nack& nack,
    const shared_ptr<pit::Entry>& pitEntry )
{
  return;
  struct QueueItem item( &pitEntry );

  //std::cout << "***Nack name: " << nack.getInterest().getName() << " Reason: "<<nack.getReason()<< std::endl;

  //std::string s = nack.getInterest().getName().getSubName( 2,1 ).toUri();

  uint32_t pr_level = getPrType(nack.getInterest().getName());

  item.wireEncode = nack.getInterest().wireEncode();
  item.packetType = NACK;
  item.inface = &inFace;
  uint32_t f = inFace.getId();
  this->processNack( inFace, nack, pitEntry );
}

void
QosStrategy::afterReceiveData( const shared_ptr<pit::Entry>& pitEntry,
                           const Face& inFace, const Data& data )
{
  struct QueueItem item( &pitEntry );

  NFD_LOG_DEBUG( "afterReceiveData pitEntry=" << pitEntry->getName() <<
      " inFace=" << inFace.getId() << " data=" << data.getName() );
  //std::cout << "***Data name: " << data.getName() << std::endl;

  this->beforeSatisfyInterest( pitEntry, inFace, data );

  //std::string s = data.getName().getSubName( 2,1 ).toUri();
  uint32_t pr_level = getPrType(data.getName());

  item.wireEncode = data.wireEncode();
  item.packetType = DATA;
  item.inface = &inFace;
  std::set<Face*> pendingDownstreams;
  auto now = time::steady_clock::now();

  // Remember pending downstreams
  for( const pit::InRecord& inRecord : pitEntry->getInRecords() ) {
    if( inRecord.getExpiry() > now ) {
      if( inRecord.getFace().getId() == inFace.getId() &&
          inRecord.getFace().getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) {
        continue;
      }
      pendingDownstreams.insert( &inRecord.getFace() );
    }
  }

  for( const Face* pendingDownstream : pendingDownstreams ) {
    uint32_t f = ( *pendingDownstream ).getId();
    item.outface = pendingDownstream;
       if (m_tx_queue.find(f) == m_tx_queue.end()){
	  int size = m_successReqs.size();
          m_tx_queue[f].initialize(size);
       }
    m_tx_queue[f].DoEnqueue( item, pr_level );

  }

  prioritySend();
}

void
QosStrategy::prioritySend()
{
  if(!driverConnected){
     setUp();
     driverConnected = true;
  }
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  Interest interest;
  Data data;
  lp::Nack nack;
  double TOKEN_REQUIRED = 1;
  bool tokenwait = false;

  std::unordered_map<uint32_t,NdnPriorityTxQueue >::iterator itt = m_tx_queue.begin();

  while( itt != m_tx_queue.end() ) {

   Forwarder& fwdr = *(node->GetObject<ns3::ndn::L3Protocol>()->getForwarder());
   const Face& outFace = *(fwdr.getFaceTable().get(itt->first));
   ns3::ndn::NetDeviceTransport* device = dynamic_cast<ns3::ndn::NetDeviceTransport*>( outFace.getTransport());
   uint32_t rate =25;
   if(device != NULL){
        rate = ns3::DynamicCast<ns3::QueueBase>(ns3::DynamicCast<ns3::PointToPointNetDevice>( device->GetNetDevice())->GetQueue())->GetNPackets() ;
    }
    while (!m_tx_queue[itt->first].IsEmpty() && !tokenwait && rate <25) {

      int size = CT.drivers[node->GetId()]->getSize();
      std::vector<TokenBucket*> buckets;
      std::vector<double> tokens; 
      for (int i = 0; i < size; i++){
         buckets.push_back(CT.drivers[node->GetId()]->getBucket(i));
	 tokens.push_back(buckets[i]->m_capacity);
	 if( buckets[i]->m_tokens.find( itt->first ) != buckets[i]->m_tokens.end() )
            tokens[i] = buckets[i]->m_tokens[itt->first];
	 //std::cout<<tokens[i]<<" ";
      }

      //std::cout<<std::endl;
      int choice = m_tx_queue[itt->first].SelectQueueToSend( tokens );

      if( choice != -1 ) {
        TokenBucket *sender = buckets[choice];

        sender->hasFaces = true;
        sender->consumeToken( TOKEN_REQUIRED, itt->first );
        sender->m_need[itt->first] = 0;

        // Dequeue the packet
        struct QueueItem item = m_tx_queue[itt->first].DoDequeue( choice );
        const shared_ptr<pit::Entry>* PE = &( item.pitEntry );
        rate++;

        switch( item.packetType ) {

          case INTEREST:
            interest.wireDecode(  item.wireEncode  );
            prioritySendInterest( *( PE ), *( item.inface ), interest, ( *item.outface ) );
            break;

          case DATA:
            //std::cout<<"prioritySend( DATA )\n";
            data.wireDecode(  item.wireEncode  );
            prioritySendData( *( PE ), *( item.inface ), data, ( *item.outface ) );
            break;

          case NACK:
            //std::cout<<"prioritySend( NACK )\n";
            interest.wireDecode(  item.wireEncode  );
            nack = lp::Nack( interest );
            prioritySendNack( *( PE ), *( item.inface ), nack );
            break;

          default:
            //std::cout<<"prioritySend( Invalid Type )\n";
            break;
        }
      } 
      else {

        for (int i = 0; i < size; i++){
	   buckets[i]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReq(i);
	}

        tokenwait = true;
      }
    }

    itt++;
  }
}

void
QosStrategy::prioritySendData( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Data& data, const Face& outFace )
{
  this->sendData( pitEntry, data, outFace );
}

void
QosStrategy::prioritySendNack( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const lp::Nack& nack )
{
  this->processNack( inFace, nack, pitEntry );
}

void
QosStrategy::prioritySendInterest( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Interest& interest, const Face& outFace )
{
    this->sendInterest( pitEntry, *const_pointer_cast<Face>( outFace.shared_from_this() ), interest );
}


void
QosStrategy::initialize(std::string name, const shared_ptr<pit::Entry>& pitEntry)
{
   FaceStats newPrefix;
   m_faceStats.insert({name, newPrefix});
   const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
   const fib::NextHopList& nexthops = fibEntry.getNextHops();
   uint32_t inFaceId = pitEntry->getInRecords().front().getFace().getId();
   for( const auto& nexthop : nexthops ) {
      Face& outFace = nexthop.getFace();
  
      if( ( outFace.getId() == inFaceId  && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) ||
         wouldViolateScope( pitEntry->getInRecords().front().getFace(), pitEntry->getInterest(), outFace ) ) {
         continue;
      }
      
      uint32_t f = outFace.getId();
      m_faceStats[name].absLossRate.insert({f, 0});
      m_faceStats[name].relaLossRate.insert({f, 0});
  }

}


/**
 * @brief      Gets the face probability
 *
 * @param[in]  faceProdHash  The faceid-producer hash
 * @param[in]  deadline      The deadline
 *
 * @return     The face prob.
 */
double
QosStrategy::getFaceProb(std::string namePrefix, uint32_t f,  bool rel)
{
    if(!m_faceStats[namePrefix].bootstrapped) return 0;
    double prob;
    //calculate probability of success with given loss rate
    if(!rel) 
       prob = 1.0 - m_faceStats[namePrefix].absLossRate[f];
    else  
       prob = 1.0 - m_faceStats[namePrefix].relaLossRate[f];
    //NS_LOG_INFO("Face latency is: " << faceFlowStatTable[faceProdHash].latency << " Latency deviation is " << faceFlowStatTable[faceProdHash].latencyVariance);
    if (prob == 0)
        return 0;
    //calculate probability of success with given latency ewma and the deadline for current interest
    //prob *= gsl_cdf_gaussian_P((deadline - faceFlowStatTable[faceProdHash].latency) , pow(faceFlowStatTable[faceProdHash].latencyVariance, 2)) ;

    return prob;
}

void
QosStrategy::beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                                const Face& inFace, const Data& data)
{
   std::string namePrefix = data.getName().getSubName( 0, data.getName().size()-2).toUri();
    //check if initilized 
    if(m_faceStats.find(namePrefix) == m_faceStats.end()){
	return;
    }

   double beta = 0.833;

   uint32_t f = inFace.getId();
   m_faceStats[namePrefix].absLossRate[f] = beta  * 0. + (1. - beta ) * m_faceStats[namePrefix].absLossRate[f];
   m_faceStats[namePrefix].relaLossRate[f] = beta  * 0. + (1. - beta ) * m_faceStats[namePrefix].relaLossRate[f];

   for (const auto& out : pitEntry->getOutRecords()) {
      if(inFace.getId() == out.getFace().getId()) continue; 
      m_faceStats[namePrefix].absLossRate[out.getFace().getId()] = beta * 1. + (1. - beta ) * m_faceStats[namePrefix].absLossRate[out.getFace().getId()];
   }
}

void
QosStrategy::beforeExpirePendingInterest (const pit::Entry& pitEntry)
{
   if(wasRejected(pitEntry.getName()))
           return;
    std::string namePrefix = pitEntry.getName().getSubName( 0,pitEntry.getName().size()-2).toUri();
    //check if initilized 
    if(m_faceStats.find(namePrefix) == m_faceStats.end()){
	return;
    }
   double beta = 0.833;

   for (const auto& out : pitEntry.getOutRecords()) {
      m_faceStats[namePrefix].absLossRate[out.getFace().getId()] = beta * 1. + (1. - beta ) * m_faceStats[namePrefix].absLossRate[out.getFace().getId()];
      m_faceStats[namePrefix].absLossRate[out.getFace().getId()] = beta * 1. + (1. - beta ) * m_faceStats[namePrefix].relaLossRate[out.getFace().getId()];
   }


} 

} // namespace fw
} // namespace nfd
