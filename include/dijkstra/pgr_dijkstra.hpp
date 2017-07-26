/*PGR-GNU*****************************************************************

Copyright (c) 2015 pgRouting developers
Mail: project@pgrouting.org

Copyright (c) 2015 Celia Virginia Vergara Castillo
vicky_vergara@hotmail.com

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

#ifndef INCLUDE_DIJKSTRA_PGR_DIJKSTRA_HPP_
#define INCLUDE_DIJKSTRA_PGR_DIJKSTRA_HPP_
#pragma once

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/dijkstra_shortest_paths_no_color_map.hpp>

#include <deque>
#include <set>
#include <vector>
#include <algorithm>

#include "cpp_common/basePath_SSEC.hpp"
#include "cpp_common/pgr_base_graph.hpp"
#if 0
#include "./../../common/src/signalhandler.h"
#endif


template < class G > class Pgr_dijkstra;
// user's functions
// for development

template < class G >
std::deque<Path>
pgr_drivingDistance(
        G &graph,
        std::vector< int64_t > start_vids,
        double distance,
        bool equicost) {
    Pgr_dijkstra< G > fn_dijkstra;
    return fn_dijkstra.drivingDistance(graph, start_vids, distance, equicost);
}


/* 1 to 1*/
template < class G >
Path
pgr_dijkstra(
        G &graph,
        int64_t source,
        int64_t target,
        bool only_cost = false) {
    Pgr_dijkstra< G > fn_dijkstra;
    return fn_dijkstra.dijkstra(graph, source, target, only_cost);
}




//******************************************

template < class G >
class Pgr_dijkstra {
 public:
     typedef typename G::V V;

     //! @name drivingDistance
     //@{
     //! 1 to distance
     Path drivingDistance(
             G &graph,
             int64_t start_vertex,
             double distance) {
        if (execute_drivingDistance(
                graph,
                start_vertex,
                distance)) {
            auto path = Path(graph, start_vertex, distance, predecessors, distances);

            std::sort(path.begin(), path.end(),
                    [](const Path_t &l, const  Path_t &r)
                    {return l.node < r.node;});
            std::stable_sort(path.begin(), path.end(),
                    [](const Path_t &l, const  Path_t &r)
                    {return l.agg_cost < r.agg_cost;});
            return path;
        }

        /* The result is empty */
        Path p(start_vertex, start_vertex);
        p.push_back({start_vertex, -1, 0, 0});
        return p;
     }

     // preparation for many to distance
     std::deque< Path > drivingDistance(
             G &graph,
             const std::vector< int64_t > start_vertex,
             double distance,
             bool equicost) {
         return equicost?
             drivingDistance_with_equicost(
                     graph,
                     start_vertex,
                     distance)
             :
             drivingDistance_no_equicost(
                     graph,
                     start_vertex,
                     distance);
     }






     //@}

     //! @name Dijkstra
     //@{
     //! one to one
     Path dijkstra(
             G &graph,
             int64_t start_vertex,
             int64_t end_vertex,
             bool only_cost = false);

     //! Many to one
     std::deque<Path> dijkstra(
             G &graph,
             const std::vector < int64_t > &start_vertex,
             int64_t end_vertex,
             bool only_cost = false);

     //! Many to Many
     std::deque<Path> dijkstra(
             G &graph,
             const std::vector< int64_t > &start_vertex,
             const std::vector< int64_t > &end_vertex,
             bool only_cost = false);

     //! one to Many
     std::deque<Path> dijkstra(
             G &graph,
             int64_t start_vertex,
             const std::vector< int64_t > &end_vertex,
             bool only_cost = false);
     //@}

 private:
     //! Call to Dijkstra  1 source to 1 target
     bool dijkstra_1_to_1(
                 G &graph,
                 V source,
                 V target) {
         bool found = false;
         try {
             boost::dijkstra_shortest_paths(graph.graph, source,
                     boost::predecessor_map(&predecessors[0])
                     .weight_map(get(&G::G_T_E::cost, graph.graph))
                     .distance_map(&distances[0])
                     .visitor(dijkstra_one_goal_visitor(target)));
         }
         catch(found_goals &) {
             found = true;  // Target vertex found
         } catch (...) {
         }
         return found;
     }

     /** Call to Dijkstra  1 to distance
      *
      * Used on:
      *   1 to distance
      *   many to distance
      *   On the first call of many to distance with equi_cost
      */
     bool dijkstra_1_to_distance(
             G &graph,
             V source,
             double distance) {
         bool found = false;
         try {
             boost::dijkstra_shortest_paths(graph.graph, source,
                     boost::predecessor_map(&predecessors[0])
                     .weight_map(get(&G::G_T_E::cost, graph.graph))
                     .distance_map(&distances[0])
                     .visitor(dijkstra_distance_visitor(
                             distance,
                             nodesInDistance,
                             distances)));
         } catch(found_goals &) {
             found = true;
         } catch (...) {
         }
         return found;
     }

     /** Call to Dijkstra  1 to distance no init
      *
      * Used on:
      *   On the subsequent calls of many to distance with equi_cost
      */
     bool dijkstra_1_to_distance_no_init(
             G &graph,
             V source,
             double distance) {
         bool found = false;
         try {
             /*
              *   void dijkstra_shortest_paths_no_color_map_no_init
              *       (const Graph& graph,
              *            typename graph_traits<Graph>::vertex_descriptor start_vertex,
              *                 PredecessorMap predecessor_map,
              *                      DistanceMap distance_map,
              *                           WeightMap weight_map,
              *                                VertexIndexMap index_map,
              *                                     DistanceCompare distance_compare,
              *                                          DistanceWeightCombine distance_weight_combine,
              *                                               DistanceInfinity distance_infinity,
              *                                                    DistanceZero distance_zero,
              *                                                         DijkstraVisitor visitor)
              
              The solution is just to: 1) Say the vertex descriptor is defined as typedef Graph::vertex_descriptor G::G_T_V;i
              then you need to define an associative property map as following:




             std::vector<V> indexMap(graph.m_num_vertices) ; // vector with 100 ints.
             std::iota(std::begin(indexMap), std::end(indexMap), 0); 
             std::vector<V> predecessors(num_vertices(graph.graph));
              */

             typedef typename boost::property_map < typename G::B_G, size_t >::type IndexMap;
             IndexMap indexMap = boost::get(&G::G_T_E::id, graph.graph);
#if 1
             boost::dijkstra_shortest_paths_no_init(
                     graph.graph,
                     source,
                     boost::predecessor_map(&predecessors[0]),
                     boost::distance_map(&distances[0]),
                     weight_map(get(&G::G_T_E::cost, graph.graph)),
                     get(boost::vertex_index, graph.graph),
                     std::less<double>(),
                     boost::closed_plus<double>(), 
                     double(0),  //DistZero
                     visitor(dijkstra_distance_visitor(
                             distance,
                             nodesInDistance,
                             distances)));
#endif
         } catch(found_goals &) {
             found = true;
         } catch (...) {
         }
         return found;
     }


     /** @brief to use with driving distance
      *
      * Prepares the execution for a driving distance:
      *
      * @params graph
      * @params start_vertex
      * @params distance
      *
      * Results are kept on predecessor & distances
      *
      * @returns bool  @b True when results are found
      */
     bool execute_drivingDistance(
             G &graph,
             int64_t start_vertex,
             double distance) {
         clear();

         predecessors.resize(graph.num_vertices());
         distances.resize(graph.num_vertices());

         // get source;
         if (!graph.has_vertex(start_vertex)) {
             return false;
         }

         return dijkstra_1_to_distance(
                 graph,
                 graph.get_V(start_vertex),
                 distance);
     }


     /** @brief to use with driving distance
      *
      * Prepares the execution for a driving distance:
      *
      * @params graph
      * @params start_vertex
      * @params distance
      *
      * Results are kept on predecessor & distances
      *
      * @returns bool  @b True when results are found
      */
     bool execute_drivingDistance_no_init(
             G &graph,
             int64_t start_vertex,
             double distance) {
#if 0
        predecessors.clear();
        predecessors.resize(graph.num_vertices());
#endif
         // get source;
         if (!graph.has_vertex(start_vertex)) {
             return false;
         }

         return dijkstra_1_to_distance_no_init(
                 graph,
                 graph.get_V(start_vertex),
                 distance);
     }

     /* preparation for many to distance with equicost
      *
      * Idea:
      *   The distances vector does not change
      *   The predecessors vector does not change
      *   The first @b valid execution is done normally:
      *     - The distances will have:
      *       - inf
      *       - values < distance
      *       - values > distance
      *   Subsequent @b valid executions
      *       - will not change the:
      *         - values < distance
      *   Dont know yet what happens to predecessors
      */
     std::deque< Path > drivingDistance_with_equicost(
             G &graph,
             const std::vector< int64_t > start_vertex,
             double distance) {
#if 1
         clear();

         predecessors.resize(graph.num_vertices());
         distances.resize(graph.num_vertices());

         std::deque< std::vector< V > > pred;
         std::deque< std::vector< double > > dist;

         bool first = true;
         // perform the algorithm
         for (const auto &vertex : start_vertex) {
             if (first) { 
                 if (execute_drivingDistance(graph, vertex, distance)) {
                     pred.push_back(predecessors);
                     dist.push_back(distances);
                     first = false;
                 } else {
                     pred.push_back(std::vector< V >());
                     dist.push_back(std::vector< double >());
                 }
             } else {
                 if (execute_drivingDistance_no_init(graph, vertex, distance)) {
                     pred.push_back(predecessors);
                     dist.push_back(distances);
                 } else {
                     pred.push_back(std::vector< V >());
                     dist.push_back(std::vector< double >());
                 }
             }
         }
         auto path = Path(graph, 1, distance, predecessors, distances);
         std::sort(path.begin(), path.end(),
                 [](const Path_t &l, const  Path_t &r)
                 {return l.node < r.node;});
         std::stable_sort(path.begin(), path.end(),
                 [](const Path_t &l, const  Path_t &r)
                 {return l.agg_cost < r.agg_cost;});
         std::deque<Path> paths;
         paths.push_back(path);

#else
         std::deque< std::vector< V > > pred;
         std::deque< std::vector< double > > dist;

         // perform the algorithm
         for (const auto &vertex : start_vertex) {
             if (execute_drivingDistance(graph, vertex, distance)) {
                 pred.push_back(predecessors);
                 dist.push_back(distances);
             } else {
                 pred.push_back(std::vector< V >());
                 dist.push_back(std::vector< double >());
             }
         }
         std::deque<Path> paths;
         for (const auto source : start_vertex) {
             if (pred.front().empty()) {
                 Path p(source, source);
                 p.push_back({source, -1, 0, 0});
                 paths.push_back(p);
             } else {
                 auto path = Path(graph, source, distance, pred.front(), dist.front());
                 std::sort(path.begin(), path.end(),
                         [](const Path_t &l, const  Path_t &r)
                         {return l.node < r.node;});
                 std::stable_sort(path.begin(), path.end(),
                         [](const Path_t &l, const  Path_t &r)
                         {return l.agg_cost < r.agg_cost;});
                 paths.push_back(path);
             }
             pred.pop_front();
             dist.pop_front();
         }
         equi_cost(paths);
#endif
         return paths;
     }

     // preparation for many to distance No equicost
     std::deque< Path > drivingDistance_no_equicost(
             G &graph,
             const std::vector< int64_t > start_vertex,
             double distance) {
         std::deque< std::vector< V > > pred;
         std::deque< std::vector< double > > dist;

         // perform the algorithm
         std::deque<Path> paths;
         for (const auto &vertex : start_vertex) {
             if (execute_drivingDistance(graph, vertex, distance)) {
                 auto path = Path(graph, vertex, distance, predecessors, distances);
                 std::sort(path.begin(), path.end(),
                         [](const Path_t &l, const  Path_t &r)
                         {return l.node < r.node;});
                 std::stable_sort(path.begin(), path.end(),
                         [](const Path_t &l, const  Path_t &r)
                         {return l.agg_cost < r.agg_cost;});
                 paths.push_back(path);
             } else {
                 Path p(vertex, vertex);
                 p.push_back({vertex, -1, 0, 0});
                 paths.push_back(p);
             }
         }
         return paths;
     }


     //! Call to Dijkstra  1 source to many targets
     bool dijkstra_1_to_many(
             G &graph,
             V source,
             const std::vector< V > &targets) {
         bool found = false;
         try {
             boost::dijkstra_shortest_paths(graph.graph, source,
                     boost::predecessor_map(&predecessors[0])
                     .weight_map(get(&G::G_T_E::cost, graph.graph))
                     .distance_map(&distances[0])
                     .visitor(dijkstra_many_goal_visitor(targets)));
         } catch(found_goals &) {
             found = true;  // Target vertex found
         } catch (...) {
         }
         return found;
     }


     void clear() {
         predecessors.clear();
         distances.clear();
         nodesInDistance.clear();
     }




     // used when multiple goals
     std::deque<Path> get_paths(
             const G &graph,
             V source,
             std::vector< V > &targets,
             bool only_cost) const {
         std::deque<Path> paths;
         for (const auto target : targets) {
             paths.push_back(Path(
                         graph,
                         source, target,
                         predecessors, distances,
                         only_cost, true));
         }
         return paths;
     }



     //! @name members
     //@{
     struct found_goals{};  //!< exception for termination
     std::vector< V > predecessors;
     std::vector< double > distances;
     std::deque< V > nodesInDistance;
     //@}

     //! @name Stopping classes
     //@{
     //! class for stopping when 1 target is found
     class dijkstra_one_goal_visitor : public boost::default_dijkstra_visitor {
      public:
          explicit dijkstra_one_goal_visitor(V goal) : m_goal(goal) {}
          template <class B_G>
              void examine_vertex(V &u, B_G &g) {
                  if (u == m_goal) throw found_goals();
                  num_edges(g);
              }
      private:
          V m_goal;
     };

     //! class for stopping when all targets are found
     class dijkstra_many_goal_visitor : public boost::default_dijkstra_visitor {
      public:
          explicit dijkstra_many_goal_visitor(std::vector< V > goals)
              :m_goals(goals.begin(), goals.end()) {}
          template <class B_G>
              void examine_vertex(V u, B_G &g) {
                  auto s_it = m_goals.find(u);
                  if (s_it == m_goals.end()) return;
                  // we found one more goal
                  m_goals.erase(s_it);
                  if (m_goals.size() == 0) throw found_goals();
                  num_edges(g);
              }
      private:
          std::set< V > m_goals;
     };


     //! class for stopping when a distance/cost has being surpassed
     class dijkstra_distance_visitor : public boost::default_dijkstra_visitor {
      public:
          explicit dijkstra_distance_visitor(
                  double distance_goal,
                  std::deque< V > &nodesInDistance,
                  std::vector< double > &distances) :
              m_distance_goal(distance_goal),
              m_nodes(nodesInDistance),
              m_dist(distances) {
              }
          template <class B_G>
              void examine_vertex(V u, B_G &g) {
                  m_nodes.push_back(u);
                  if (m_dist[u] >= m_distance_goal) throw found_goals();
                  num_edges(g);
              }
      private:
          double m_distance_goal;
          std::deque< V > &m_nodes;
          std::vector< double > &m_dist;
     };


     //@}
};


/******************** IMPLEMENTTION ******************/





//! Dijkstra 1 to 1
template < class G >
Path
Pgr_dijkstra< G >::dijkstra(
        G &graph,
        int64_t start_vertex,
        int64_t end_vertex,
        bool only_cost) {
    clear();

    // adjust predecessors and distances vectors
    predecessors.resize(graph.num_vertices());
    distances.resize(graph.num_vertices());


    if (!graph.has_vertex(start_vertex)
            || !graph.has_vertex(end_vertex)) {
        return Path(start_vertex, end_vertex);
    }

    // get the graphs source and target
    auto v_source(graph.get_V(start_vertex));
    auto v_target(graph.get_V(end_vertex));

    // perform the algorithm
    dijkstra_1_to_1(graph, v_source, v_target);

    // get the results
    return Path(
            graph,
            v_source, v_target,
            predecessors, distances,
            only_cost, true);
}

//! Dijkstra 1 to many
template < class G >
std::deque<Path>
Pgr_dijkstra< G >::dijkstra(
        G &graph,
        int64_t start_vertex,
        const std::vector< int64_t > &end_vertex,
        bool only_cost) {
    // adjust predecessors and distances vectors
    clear();

    predecessors.resize(graph.num_vertices());
    distances.resize(graph.num_vertices());

    // get the graphs source and target
    if (!graph.has_vertex(start_vertex))
        return std::deque<Path>();
    auto v_source(graph.get_V(start_vertex));

    std::set< V > s_v_targets;
    for (const auto &vertex : end_vertex) {
        if (graph.has_vertex(vertex)) {
            s_v_targets.insert(graph.get_V(vertex));
        }
    }

    std::vector< V > v_targets(s_v_targets.begin(), s_v_targets.end());
    // perform the algorithm
    dijkstra_1_to_many(graph, v_source, v_targets);

    std::deque< Path > paths;
    // get the results // route id are the targets
    paths = get_paths(graph, v_source, v_targets, only_cost);

    std::stable_sort(paths.begin(), paths.end(),
            [](const Path &e1, const Path &e2)->bool {
            return e1.end_id() < e2.end_id();
            });

    return paths;
}

// preparation for many to 1
template < class G >
std::deque<Path>
Pgr_dijkstra< G >::dijkstra(
        G &graph,
        const std::vector < int64_t > &start_vertex,
        int64_t end_vertex,
        bool only_cost) {
    std::deque<Path> paths;

    for (const auto &start : start_vertex) {
        paths.push_back(
                dijkstra(graph, start, end_vertex, only_cost));
    }

    std::stable_sort(paths.begin(), paths.end(),
            [](const Path &e1, const Path &e2)->bool {
            return e1.start_id() < e2.start_id();
            });
    return paths;
}


// preparation for many to many
template < class G >
std::deque<Path>
Pgr_dijkstra< G >::dijkstra(
        G &graph,
        const std::vector< int64_t > &start_vertex,
        const std::vector< int64_t > &end_vertex,
        bool only_cost) {
    // a call to 1 to many is faster for each of the sources
    std::deque<Path> paths;
    for (const auto &start : start_vertex) {
        auto r_paths = dijkstra(graph, start, end_vertex, only_cost);
        paths.insert(paths.begin(), r_paths.begin(), r_paths.end());
    }

    std::sort(paths.begin(), paths.end(),
            [](const Path &e1, const Path &e2)->bool {
            return e1.end_id() < e2.end_id();
            });
    std::stable_sort(paths.begin(), paths.end(),
            [](const Path &e1, const Path &e2)->bool {
            return e1.start_id() < e2.start_id();
            });
    return paths;
}





#endif  // INCLUDE_DIJKSTRA_PGR_DIJKSTRA_HPP_
