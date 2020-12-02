/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright ( C ) 2020 New Mexico State University
 *
 * George Torres, Anju Kunnumpurathu James
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

#ifndef NFD_DAEMON_FW_TOKEN_BUCKET_HPP
#define NFD_DAEMON_FW_TOKEN_BUCKET_HPP

#include "strategy.hpp"
#include <unordered_map>

namespace nfd {
namespace fw {

/** \brief Helps os-stratagey keep track of token operations.
 */
class TokenBucket
{
public:

  TokenBucket();

  /** \brief Update strategy that token bucket has refilled.
   */
  signal::Signal<TokenBucket>
  send;

  /** \brief Increment token counts in buckets if they are not currently at capacity.
   */
  void
  addToken();

  /** \brief Remove tokens from token bucket for the given interface.
   *  \param tokens amount of tokens to remove.
   *  \param face interface on which to remove tokens from.
   */
  void
  consumeToken( double tokens, uint32_t face );

  bool hasFaces;
  std::unordered_map<uint32_t, double> m_tokens;
  std::unordered_map<uint32_t, double> m_need;
  int m_capacity;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_TOKEN_BUCKET_HPP
