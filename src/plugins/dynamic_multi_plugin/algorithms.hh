/**
 *   Copyright (C) 2012-2013 IFSTTAR (http://www.ifsttar.fr)
 *   Copyright (C) 2012-2013 Oslandia <infos@oslandia.com>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

// Path algorithms for combined graphs with homogen labeling strategy

#include <boost/pending/indirect_cmp.hpp>
#include <boost/heap/d_ary_heap.hpp>
#include <boost/heap/binomial_heap.hpp>

namespace Tempus {

    //
    // Implementation of the Dijkstra algorithm (label-setting) for a graph and an automaton
    template < class NetworkGraph,
               class Automaton,
               class Object, 
               class PredecessorMap, 
               class PotentialMap,
               class CostCalculator, 
               class TripMap, 
               class WaitMap, 
               class Visitor >
    void combined_ls_algorithm_no_init(
                                       const NetworkGraph& graph,
                                       const Automaton& automaton,
                                       Object source_object, 
                                       PredecessorMap predecessor_map, 
                                       PotentialMap potential_map,
                                       CostCalculator cost_calculator, 
                                       TripMap trip_map, 
                                       WaitMap wait_map, 
                                       const std::vector<db_id_t>& request_allowed_modes,
                                       Visitor vis) 
    {
        typedef boost::indirect_cmp< PotentialMap, std::greater<double> > Cmp; 
        Cmp cmp( potential_map ); 
		
        typedef boost::heap::d_ary_heap< Object, boost::heap::arity<4>, boost::heap::compare< Cmp >, boost::heap::mutable_<true> > VertexQueue;  
        VertexQueue vertex_queue( cmp ); 
        vertex_queue.push( source_object ); 
		
        Object min_object; 

        while ( !vertex_queue.empty() ) {
            min_object = vertex_queue.top();
            vis.examine_vertex( min_object, graph );
            vertex_queue.pop();

            double min_pi = get( potential_map, min_object );
			
            //vis.examine_vertex( min_object, graph );
			
            BGL_FORALL_OUTEDGES_T( min_object.vertex, current_edge, graph, NetworkGraph ) {
                for ( size_t i = 0; i < request_allowed_modes.size(); i++ ) {
                    db_id_t mode_id = request_allowed_modes[i];
                    boost::optional<TransportMode> mode;
                    mode = graph.transport_mode(mode_id);
                    BOOST_ASSERT(mode);

                    // if this mode is not allowed on the current edge, skip it
                    if ( ! (current_edge.traffic_rules() & mode->traffic_rules() ) ) {
                        continue;
                    }

                    typename Automaton::State s = min_object.state;
                    {
                        bool found;
                        boost::tie( s, found ) = automaton.find_transition( min_object.state, current_edge.road_edge() );
                        // if not found, s == min_object.state
                    }
                    Object new_object;
                    new_object.vertex = target(current_edge, graph);
                    new_object.mode = mode_id;
                    new_object.state = s;

                    double new_pi = get( potential_map, new_object );
                    db_id_t initial_trip_id = get( trip_map, min_object );
                    db_id_t final_trip_id ;
                    double wait_time;

                    // compute the time needed to transfer from one mode to another
                    double cost = cost_calculator.transfer_time( graph, min_object.vertex, new_object.vertex, min_object.mode, new_object.mode );
                    if ( cost < std::numeric_limits<double>::max() )
                    {
                        // will update final_trip_id and wait_time
                        double travel_time = cost_calculator.travel_time( graph,
                                                             current_edge,
                                                             mode_id,
                                                             min_pi,
                                                             initial_trip_id,
                                                             final_trip_id,
                                                             wait_time );
                        cost += travel_time;
                        if ( ( cost < std::numeric_limits<double>::max() ) && ( s != min_object.state ) ) {
                            cost += penalty( automaton.automaton_graph_, s, mode->traffic_rules() ) ;
                        }
                    }

                    if ( ( cost < std::numeric_limits<double>::max() ) && ( min_pi + cost < new_pi ) ) {
                        put( potential_map, new_object, min_pi + cost ); 
                        put( trip_map, new_object, final_trip_id ); 

                        put( predecessor_map, new_object, min_object );
                        put( wait_map, min_object, wait_time ); 

                        vertex_queue.push( new_object ); 

                        //vis.edge_relaxed( current_edge, mode_label, graph ); 
                    }
                }
            }

            //vis.finish_vertex( min_object, graph ); 
        }
    }
	
}// end namespace
