/*PGR-GNU*****************************************************************
File: pgr_details.h

Copyright (c) 2018 Vicky Vergara


------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
********************************************************************PGR-GNU*/

#ifndef INCLUDE_MST_DETAILS_HPP_
#define INCLUDE_MST_DETAILS_HPP_
#pragma once

#include <vector>
#include "c_types/pgr_mst_rt.h"

namespace pgrouting {

struct found_goals{}; //!< exception for dfs termination

namespace details {

std::vector<int64_t>
clean_vids(std::vector<int64_t> vids);

std::vector<pgr_mst_rt>
get_no_edge_graph_result(
        std::vector<int64_t> vids);

}  // namespace details
}  // namespace pgrouting

#endif  // INCLUDE_MST_DETAILS_HPP_
