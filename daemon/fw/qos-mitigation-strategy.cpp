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

#include "qos-mitigation-strategy.hpp"
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

NFD_REGISTER_STRATEGY( QosMitigation );

NFD_LOG_INIT( QosMitigation );


QosMitigation::QosMitigation( Forwarder& forwarder, const Name& name )
  : QosStrategy( forwarder )
{
  //CT.m_tokens = 0;
  ParsedInstanceName parsed = parseInstanceName( name );

  if( !parsed.parameters.empty() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( "QosMitigation does not accept parameters" ) );
  }

  if( parsed.version && *parsed.version != getStrategyName()[-1].toVersion() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( 
          "QosMitigation does not support version " + to_string( *parsed.version ) ) );
  }

  this->setInstanceName( makeInstanceName( name, getStrategyName() ) );
  forwarder.beforeExpirePendingInterest.connect([this](const pit::Entry& entry){
       this->beforeExpirePendingInterest(entry);
  });
  setSuccessReqs({1.0, 1.0, 0.4, 0});

}

void
QosMitigation::setUp(){

  int node= ns3::Simulator::GetContext();	
  QosStrategy::setUp();
  //if(!CT.drivers[node]->isSet()){
     CT.drivers[node]->setSize(4);
     CT.drivers[node]->addTokenBucket(&m_sender4);
     m_sender4.send.connect( [this]() {
        this->prioritySend();
       } );
  //}
}

const Name&
QosMitigation::getStrategyName()
{
  static Name strategyName( "/localhost/nfd/strategy/qos-mit/%FD%03" );
  return strategyName;
}

int
QosMitigation::getPrType(Name pkt)
{
   Name namePrefix = pkt.getSubName( 0,pkt.size()-1);
   if(monitored.find(namePrefix) != monitored.end() && monitored[namePrefix] > 0.8) return 3;
	
   return QosStrategy::getPrType(pkt);
}

void
QosMitigation::beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                                const Face& inFace, const Data& data)
{
      Name namePrefix = pitEntry->getName().getSubName( 0,pitEntry->getName().size()-1);
      lastHundred.push_back(namePrefix);
      if(lastHundred.size() > 100) lastHundred.erase(lastHundred.begin());

      if(monitored.find(namePrefix) != monitored.end()){
         monitored[namePrefix] = beta * 0. + (1. - beta ) * monitored[namePrefix];
      }
      else totalLossRate = beta * 0. + (1. - beta ) * totalLossRate;
      QosStrategy::beforeSatisfyInterest(pitEntry, inFace, data);
}

void
QosMitigation::beforeExpirePendingInterest (const pit::Entry& pitEntry)
{
   if(wasRejected(pitEntry.getName()))
	   return;
   Name namePrefix = pitEntry.getName().getSubName( 0,pitEntry.getName().size()-1);
   lastHundred.push_back(namePrefix);
   if(lastHundred.size() > 100) lastHundred.erase(lastHundred.begin());
      
   if(monitored.find(namePrefix) != monitored.end()){
      monitored[namePrefix] = beta * 1. + (1. - beta ) * monitored[namePrefix];
    }
    else {totalLossRate = beta * 1. + (1. - beta ) * totalLossRate;
      // std::cout<<"Loss tots "<<totalLossRate<<std::endl;
    }

    if (totalLossRate > 0.6 && lastHundred.size() == 100) findSusFlow();
    QosStrategy::beforeExpirePendingInterest (pitEntry);
} 


void
QosMitigation::findSusFlow ()
{
   totalLossRate = 0;
   unordered_map<Name, int> pop_map;
   int maxCount = 0;
   Name mostPopName = "";
   for (int i = 0; i < lastHundred.size(); i++){
      pop_map[lastHundred[i]]++;
      if (pop_map[lastHundred[i]] > maxCount){
         maxCount = pop_map[lastHundred[i]];
	 mostPopName = lastHundred[i];
      }
   }
   monitored[mostPopName] = 0;
   lastHundred.clear();
}
} // namespace fw
} // namespace nfd
