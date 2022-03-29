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

#include "ndn-priority-tx-queue.hpp"

NS_LOG_COMPONENT_DEFINE( "ndn.NdnPriorityTxQueue" );

namespace nfd {
namespace fw {

NdnPriorityTxQueue::NdnPriorityTxQueue()
{
}

void
NdnPriorityTxQueue::initialize(int queues){
  for (int i = 0; i < queues; i++){
     m_priorityQueues.push_back(QosQueue());
     m_priorityQueues[i].SetWeight(queues-i);
  }
  totalQueues=queues;
}


float 
NdnPriorityTxQueue::GetFlowRate( QosQueue *queue )
{
  float total_weight = 0;
  for  (int i = 0; i < totalQueues; i++){
     total_weight += m_priorityQueues[i].GetWeight();
  }
  float flowRate = queue->GetWeight() / total_weight;

  return flowRate;
}

void
NdnPriorityTxQueue::UpdateTime( ndn::Block packet, QosQueue *queue )
{
  uint64_t current_time = ns3::Simulator::Now().GetMilliSeconds();
  uint64_t virtualStartTime = std::max( current_time, queue->GetLastVirtualFinishTime() );
  uint64_t virtualFinishTime = virtualStartTime + ( packet.size() * ( 1 - GetFlowRate( queue ) ) );

  queue->SetLastVirtualFinishTime( virtualFinishTime );
}

int
NdnPriorityTxQueue::SelectQueueToSend( vector<double> tokens )
{
  int squeue = -1;
  QosQueue* selected_queue = &m_priorityQueues[0];
  float minVirtualFinishTime = std::numeric_limits<float>::max();
  //vector<double> tokens = {highTokens, midTokens, lowTokens};
  for  (int i = 0; i < totalQueues; i++){
    if( m_priorityQueues[i].IsEmpty() == false && tokens[i] >= 1 ) {
       if( minVirtualFinishTime > m_priorityQueues[i].GetLastVirtualFinishTime() ) {	    
          minVirtualFinishTime = m_priorityQueues[i].GetLastVirtualFinishTime();
          selected_queue = &m_priorityQueues[i];
          squeue = i;
       }
     }
  }
  return squeue;
}

bool
NdnPriorityTxQueue::DoEnqueue( QueueItem item, uint32_t pr_level )
{
  QosQueue *queue;
  queue = &m_priorityQueues[pr_level];
  if( queue->Enqueue( item ) ) {
    UpdateTime( item.wireEncode, queue );
    return true;
  } 
  else {
    return false;
  }
}

QueueItem
NdnPriorityTxQueue::DoDequeue( int choice )
{
  QueueItem item;

  if( !IsEmpty() ) {
     item = m_priorityQueues[choice].Dequeue();
  } else {
    //std::cout << "DoDequeue failed. All queues are empty !!!!" << std::endl;
  }

  return item;
}

bool
NdnPriorityTxQueue::IsEmpty()
{

  bool empty = true; 
  for  (int i = 0; i < totalQueues; i++){
     empty = empty &&  m_priorityQueues[i].IsEmpty();
  }
  return empty;
}

int
NdnPriorityTxQueue::tokenReq(int bucket)
{
  if( m_priorityQueues[bucket].IsEmpty() == false ) {
    return 1;
  }

  return 0;
}


} // namespace ndn
} // namespace ns3

