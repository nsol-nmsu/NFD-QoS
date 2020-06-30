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

NS_LOG_COMPONENT_DEFINE ("ndn.NdnPriorityTxQueue");

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
    uint64_t current_time = ns3::Simulator::Now().GetMilliSeconds();
    uint64_t virtualStartTime = std::max(current_time, queue->GetLastVirtualFinishTime());
    uint64_t virtualFinishTime = virtualStartTime + (packet.size() * (1 - GetFlowRate(queue)));
    queue->SetLastVirtualFinishTime( virtualFinishTime );
}

int
NdnPriorityTxQueue::SelectQueueToSend(double highTokens, double midTokens, double lowTokens)
{
    int squeue = -1;
    QosQueue* selected_queue = &m_highPriorityQueue;
    float minVirtualFinishTime = std::numeric_limits<float>::max();

    if (m_highPriorityQueue.IsEmpty() == false && highTokens > 7)
    {
        //std::cout << "m_highPriorityQueue.GetLastVirtualFinishTime(): "<<m_mediumPriorityQueue.GetLastVirtualFinishTime() << std::endl;
        minVirtualFinishTime = m_highPriorityQueue.GetLastVirtualFinishTime();
        selected_queue = &m_highPriorityQueue;
        squeue = 0;
    }

    if (m_mediumPriorityQueue.IsEmpty() == false && midTokens > 6)
    {
        //std::cout << "m_mediumPriorityQueue.GetLastVirtualFinishTime(): "<<m_mediumPriorityQueue.GetLastVirtualFinishTime() << std::endl;
        if (minVirtualFinishTime > m_mediumPriorityQueue.GetLastVirtualFinishTime())
        {
            minVirtualFinishTime = m_mediumPriorityQueue.GetLastVirtualFinishTime();
            selected_queue = &m_mediumPriorityQueue;
            squeue = 1;

        }
    }

    if (m_lowPriorityQueue.IsEmpty() == false && lowTokens > 5)
    {
        if (minVirtualFinishTime > m_lowPriorityQueue.GetLastVirtualFinishTime())
        {
            //std::cout << "m_lowPriorityQueue.GetLastVirtualFinishTime(): "<<m_mediumPriorityQueue.GetLastVirtualFinishTime() << std::endl;
            minVirtualFinishTime = m_lowPriorityQueue.GetLastVirtualFinishTime();
            selected_queue = &m_lowPriorityQueue;
            squeue = 2;
        }
    }

    return squeue;
}

bool
NdnPriorityTxQueue::DoEnqueue(QueueItem item, uint32_t dscp_value)
{
    QosQueue *queue;

    if (dscp_value >= 1 && dscp_value <= 20)
    {
        //std::cout << "Dscp value: " << dscp_value << " **** High priority" << std::endl;
        queue = &m_highPriorityQueue;

    } else if (dscp_value >= 21 && dscp_value <= 40)
    {
        //std::cout << "Dscp value: " << dscp_value << " **** Medium priority" << std::endl;
        queue = &m_mediumPriorityQueue;

    }else if (dscp_value >= 41 && dscp_value <= 64)
    {
        //std::cout << "Dscp value: " << dscp_value << " **** Low priority" << std::endl;
        queue = &m_lowPriorityQueue;

    } else
    {
        return false;
        //std::cout << "Incorrect dscp value !! Enqueue failed !!!! ";
    }

    if (queue->Enqueue(item)){
        UpdateTime(item.wireEncode, queue);
        return true;
    } else {
        return false;
    }
}

QueueItem
NdnPriorityTxQueue::DoDequeue(int choice)
{
    QueueItem item;

    if ((!m_highPriorityQueue.IsEmpty()) || (!m_mediumPriorityQueue.IsEmpty()) || (!m_lowPriorityQueue.IsEmpty()))
    {
        //QosQueue* queue = SelectQueueToSend();
        if (choice == 0) {
            item = m_highPriorityQueue.Dequeue();
        } else if (choice == 1) {
            item = m_mediumPriorityQueue.Dequeue();
        } else { item = m_lowPriorityQueue.Dequeue();
        }

    } else
    {
        //std::cout << "DoDequeue failed. All queues are empty !!!!" << std::endl;
    }

    return item;
}
bool
NdnPriorityTxQueue::IsEmpty()
{
  return (m_highPriorityQueue.IsEmpty() && m_mediumPriorityQueue.IsEmpty() && m_lowPriorityQueue.IsEmpty());
}

int
NdnPriorityTxQueue::tokenReqHig()
{
    if (m_highPriorityQueue.IsEmpty() == false) {
        return 7;
    }
    return 0;
}

int
NdnPriorityTxQueue::tokenReqMid()
{
    if (m_mediumPriorityQueue.IsEmpty() == false) {
        return 6;
    }
    return 0;
}

int
NdnPriorityTxQueue::tokenReqLow()
{
    if (m_lowPriorityQueue.IsEmpty() == false ) {
        return 5;
    }
    return 0;
}

} // namespace ndn
} // namespace ns3

