/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ndn-priority-tx-queue.hpp"

//NS_LOG_COMPONENT_DEFINE ("ndn.NdnPriorityTxQueue");

namespace nfd {
namespace fw {

NdnPriorityTxQueue::NdnPriorityTxQueue()
{
    m_highPriorityQueue.SetWeight(3);
    m_mediumPriorityQueue.SetWeight(2);
    m_lowPriorityQueue.SetWeight(1);
}

float 
NdnPriorityTxQueue::GetFlowRate(QosQueue *queue)
{
    float total_weight = m_highPriorityQueue.GetWeight() + m_lowPriorityQueue.GetWeight() + m_mediumPriorityQueue.GetWeight();
    float flowRate = queue->GetWeight() / total_weight;

    return flowRate;
}

void
NdnPriorityTxQueue::UpdateTime(ndn::Block packet, QosQueue *queue)
{
    //uint64_t current_time = Now().GetMilliSeconds();
    float current_time = 3;//TODO:Remove this and use above line
    float virtualStartTime = std::max(current_time, queue->GetLastVirtualFinishTime());

    //TODO:Get the size of packet/wireEncode instead of PACKET_SIZE
    float virtualFinishTime = virtualStartTime + (PACKET_SIZE * (1 - GetFlowRate(queue)));
    queue->SetLastVirtualFinishTime( virtualFinishTime );
}

QosQueue*
NdnPriorityTxQueue::SelectQueueToSend()
{
    QosQueue* selected_queue = &m_highPriorityQueue;
    float minVirtualFinishTime = std::numeric_limits<float>::max();

    if(m_highPriorityQueue.IsEmpty() == false )
    {
        minVirtualFinishTime = m_highPriorityQueue.GetLastVirtualFinishTime();
        selected_queue = &m_highPriorityQueue;
    }

    if(m_mediumPriorityQueue.IsEmpty() == false )
    {
        std::cout << "m_mediumPriorityQueue.GetLastVirtualFinishTime(): "<<m_mediumPriorityQueue.GetLastVirtualFinishTime() << std::endl;
        if(minVirtualFinishTime > m_mediumPriorityQueue.GetLastVirtualFinishTime())
        {
            minVirtualFinishTime = m_mediumPriorityQueue.GetLastVirtualFinishTime();
            selected_queue = &m_mediumPriorityQueue;
        }
    }

    if(m_lowPriorityQueue.IsEmpty() == false )
    {
        if(minVirtualFinishTime > m_lowPriorityQueue.GetLastVirtualFinishTime())
        {
            minVirtualFinishTime = m_lowPriorityQueue.GetLastVirtualFinishTime();
            selected_queue = &m_lowPriorityQueue;
        }
    }

    return selected_queue;
}

void
NdnPriorityTxQueue::DoEnqueue(QueueItem item, uint32_t dscp_value)
{
    QosQueue *queue;

    if(dscp_value >= 1 && dscp_value <= 20)
    {
        std::cout << "Dscp value: " << dscp_value << " **** High priority" << std::endl;
        queue = &m_highPriorityQueue;

    } else if(dscp_value >= 21 && dscp_value <= 40)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Medium priority" << std::endl;
        queue = &m_mediumPriorityQueue;

    }else if(dscp_value >= 41 && dscp_value <= 64)
    {
        std::cout << "Dscp value: " << dscp_value << " **** Low priority" << std::endl;
        queue = &m_lowPriorityQueue;

    } else
    {
        std::cout << "Incorrect dscp value !! Enqueue failed !!!! ";
    }

    queue->Enqueue(item);
    UpdateTime(item.wireEncode, queue);
}

QueueItem
NdnPriorityTxQueue::DoDequeue()
{
    QueueItem item;

    if((!m_highPriorityQueue.IsEmpty()) || (!m_mediumPriorityQueue.IsEmpty()) || (!m_lowPriorityQueue.IsEmpty()))
    {
        QosQueue* queue = SelectQueueToSend();
        item = queue->Dequeue();

    } else
    {
        std::cout << "DoDequeue failed. All queues are empty !!!!" << std::endl;
    }

    return item;
}

} // namespace ndn
} // namespace ns3

