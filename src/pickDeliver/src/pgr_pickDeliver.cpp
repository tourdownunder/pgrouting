/*PGR-GNU*****************************************************************

FILE: pgr_pickDeliver.cpp

Copyright (c) 2015 pgRouting developers
Mail: project@pgrouting.org

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


#ifdef __MINGW32__
#include <winsock2.h>
#include <windows.h>
#endif


#include <sstream>
#include <vector>
#include <algorithm>

#include "./../../common/src/pgr_types.h"
#include "./../../common/src/pgr_assert.h"

#include "./vehicle_node.h"
#include "./vehicle_pickDeliver.h"
#include "./order.h"
#include "./solution.h"
#include "./initial_solution.h"
#include "./optimize.h"
#include "./pgr_pickDeliver.h"

namespace pgRouting {
namespace vrp {


void
Pgr_pickDeliver::solve() {
    solutions.push_back(Initial_solution(0, this));
    solutions.push_back(Initial_solution(1, this));
    solutions.push_back(Initial_solution(2, this));
    solutions.push_back(Initial_solution(3, this));
    solutions.push_back(Initial_solution(4, this));
    solutions.push_back(Initial_solution(5, this));
    solutions.push_back(Initial_solution(6, this));


    /*
     * Sorting solutions: the best is at the back
     */
    std::sort(solutions.begin(), solutions.end(), []
            (const Solution &lhs, const Solution &rhs) -> bool {
            return rhs < lhs;
            } );
}



void
Pgr_pickDeliver::get_postgres_result(std::vector< General_vehicle_orders_t > &result) const {

    solutions.back().get_postgres_result(result);

    Solution::Cost cost(solutions.back().cost());
    
    General_vehicle_orders_t aggregates({
            /*
             * Vehicle id = -1 indicates its an aggregate row
             *
             * (twv, cv, fleet, wait, duration)
             */
            -1, 
            std::get<0>(cost),    // on vehicle seq tw violations
            std::get<1>(cost), // on stop id: capacity violations
            0,                      // TODO not accounting total_travel_time 
            0,                      // not accounting arrival_travel_time 
            std::get<3>(cost), // on wait time
            0,                      // TODO not accounting service_time
            std::get<4>(cost)  // on departure_time
            });
    result.push_back(aggregates);


    for (const auto sol : solutions) {
        log << sol.tau();
    }
}



/***** Constructor *******/

Pgr_pickDeliver::Pgr_pickDeliver(
        const Customer_t *customers_data, size_t total_customers,
        int p_max_vehicles,
        double p_capacity,
        int p_max_cycles,
        std::string &error) :
    /* Set the depot to be the first ddata found */
    max_capacity(p_capacity),
    max_cycles(p_max_cycles),
    max_vehicles(p_max_vehicles),
    m_starting_site({0, customers_data[0], Tw_node::NodeType::kStart}),
    m_ending_site({0, customers_data[0], Tw_node::NodeType::kEnd}),
    m_original_data(customers_data, customers_data + total_customers) {
        std::ostringstream tmplog;
        error = "";

        log << "\n *** Constructor of problem ***\n";

        /* sort data by id */
        std::sort(m_original_data.begin(), m_original_data.end(),
                [] (const Customer_t &c1, const Customer_t &c2)
                {return c1.id < c2.id;});

        /*
         * starting node:
         * id must be 0
         */
        if (m_original_data[0].id != 0) {
            error = "Depot node not found";
            return;
        }

        m_starting_site = Vehicle_node({0, customers_data[0], Tw_node::NodeType::kStart});
        m_ending_site = Vehicle_node({1, customers_data[0], Tw_node::NodeType::kEnd});
        if (!m_starting_site.is_start()) {
            log << "DEPOT" << m_starting_site;
            error = "Illegal values found on the starting site";
            return;
        }
        pgassert(m_starting_site.is_start());
        pgassert(m_ending_site.is_end());

        m_nodes.push_back(m_starting_site);
        m_nodes.push_back(m_ending_site);


        ID order_id(0);
        ID node_id(2);
        for (const auto p : m_original_data) {
            /*
             * skip Starting site
             */
            if (p.id == 0) continue; 

            /*
             *  SAMPLE CORRECT INFORMATION
             *
             * The Pickup is 11 (pindex == 0)
             * The Deliver is 1 (dindex == 0)
             *
             * id | x  | y  | demand | etime | Ltime | Stime | pindex | dindex
             *  1 | 45 | 68 |    -10 |   912 |   967 |    90 |     11 |      0
             * 11 | 35 | 69 |     10 |   448 |   505 |    90 |      0 |      1
             *
             */ 

            /*
             *  skip deliveries
             */
            if (p.Dindex == 0) continue;

            /*
             * pickup is found
             */
            Tw_node pickup({node_id++, p, Tw_node::NodeType::kPickup});
            if (!pickup.is_pickup()) {
                log << "PICKUP" << pickup;
                tmplog << "Illegal values found on Pickup " << p.id;
                error = tmplog.str();
                return;
            }
            pgassert(pickup.is_pickup());


            /*
             *  look for corresponding the delivery of the pickup
             */
            auto deliver_ptr = std::lower_bound(m_original_data.begin(), m_original_data.end(), p,
                    [] (const Customer_t &delivery, const Customer_t &pick) -> bool
                    {return delivery.id < pick.Dindex;}
                    );

            if (deliver_ptr == m_original_data.end()
                    || deliver_ptr->id != p.Dindex) {
                tmplog << "For Pickup " <<  p.id << " the corresponding Delivery was not found";
                error = tmplog.str();
                return;
            }


            /*
             * delivery is found
             */
            Tw_node delivery(node_id++, (*deliver_ptr), Tw_node::NodeType::kDelivery);
            if (!delivery.is_delivery()) {
                log << "DELIVERY" << delivery;
                tmplog << "Illegal values found on Delivery " << deliver_ptr->id;
                error = tmplog.str();
                return;
            }
            pgassert(delivery.is_delivery());


            /*
             *  add into an order & check the order
             */
            pickup.set_Did(delivery.id());
            delivery.set_Pid(pickup.id());
            m_nodes.push_back(pickup);
            m_nodes.push_back(delivery);

            m_orders.push_back( Order(order_id, node(node_id - 2), node(node_id - 1), this) );
            pgassert(m_orders.back().pickup().is_pickup());
            pgassert(m_orders.back().delivery().is_delivery());
            pgassert(static_cast<Tw_node> (m_orders.back().pickup()) == pickup);

            /*
             *  check the (S,P,D,E) order
             */
            { 
                Vehicle_pickDeliver truck(order_id, m_starting_site, m_ending_site, max_capacity, this);
                truck.push_back(m_orders.back());

                if (!truck.is_feasable()) {
                    log << truck << "\n";
                    tmplog << "The (pickup, delivery) = ("
                        << m_orders.back().pickup().original_id() << ", "
                        << m_orders.back().delivery().original_id() << ") is not feasable";
                    error =  tmplog.str();
                    return;
                };
                pgassert(truck.is_feasable());
            }  // check

            ++order_id;
        }  // for

        /*
         *  double check we found all orders
         */
        if (((m_orders.size() * 2 + 1) - m_original_data.size()) != 0 ) {
            error =  "A pickup was not found";
            return;
        }

        for (auto &o : m_orders) {
            o.setCompatibles();
        }

        for (auto o : m_orders) {
            log << o;
        }
    }  // constructor


const Order
Pgr_pickDeliver::order_of(const Vehicle_node &node) const {
    assert(node.is_pickup() || node.is_delivery());
    if (node.is_pickup()) {
        for (const auto o : m_orders) {
            if (o.pickup().id() == node.id()) {
                return o;
            };
        }
    }
    assert(node.is_delivery());

    for (const auto o : m_orders) {
        if (o.delivery().id() == node.id()) {
            return o;
        }
    }
    assert(false);
}


const Vehicle_node&
Pgr_pickDeliver::node(ID id) const {
    assert (id < m_nodes.size());
    assert (id == m_nodes[id].id());
    return m_nodes[id];
}


}  // namespace pgRouting
}  // namespace vrp


