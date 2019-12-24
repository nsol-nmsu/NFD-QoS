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

#ifndef QOS_QUEUE_H
#define QOS_QUEUE_H

#include <list>
#include "strategy.hpp"
#include "ns3/ptr.h"
#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#define QUEUE_SIZE 20

namespace nfd {
namespace fw {

        class QosQueue
        {
            public:

                //constructor
                QosQueue ();

                //Set the queue size
                void
                SetMaxQueueSize (uint32_t size);

                //Get queue size
                uint32_t
                GetMaxQueueSize () const;

                // Push payloaded interest to rear end of queue.
                bool 
                Enqueue (shared_ptr<Interest> queEntry);

                //Pop data from front end of queue
                shared_ptr<Interest>
                Dequeue ();

                //Check whether queue is empty
                bool
                IsEmpty () const;

            public:  

                //Define Queue as a list of payloaded interest.
                typedef std::list< shared_ptr<Interest> > Queue;

            private:

                //The size of queue
                uint32_t m_maxQueueSize;
                Queue::iterator m_iterator; 
                Queue m_queue;

        };

    } // namespace fw
} // namespace nfd

#endif // QOS_QUEUE_H


