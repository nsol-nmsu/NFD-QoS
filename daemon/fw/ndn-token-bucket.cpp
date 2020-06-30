/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
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

#include "ndn-token-bucket.hpp"

namespace nfd {
namespace fw {

TokenBucket::TokenBucket()
{
    m_capacity = 0;
    hasFaces = false;
}

/*
void
TokenBucket::callSend()
{
    //std::cout<<"\nSending signal to Strategy"<<std::endl;
    send();
}
*/

void
TokenBucket::addToken()
{ 
   //if(hasFaces == false) return;
   std::unordered_map<uint32_t,double >::iterator itt = m_tokens.begin();
   bool callsend = false;

   while(itt != m_tokens.end()){
       if (itt->second < m_capacity) {
           m_tokens[itt->first]++;
       }

       if (m_need[itt->first] != 0 && m_tokens[itt->first] >= m_need[itt->first]) {
           callsend = true;
       }

       itt++;
   }

   if (callsend) {
       send();
   }
}

void
TokenBucket::consumeToken(double tokens, uint32_t face)
{
  if (m_tokens.find(face) == m_tokens.end()){
     m_tokens[face] = m_capacity;
  }

  m_tokens[face] = m_tokens[face] - tokens;
}


} // namespace fw
} // namespace nfd
