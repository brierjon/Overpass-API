#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../../template_db/block_backend.h"
#include "../../template_db/random_file.h"
#include "../core/settings.h"
#include "recurse.h"

using namespace std;

const unsigned int RECURSE_RELATION_RELATION = 1;
const unsigned int RECURSE_RELATION_BACKWARDS = 2;
const unsigned int RECURSE_RELATION_WAY = 3;
const unsigned int RECURSE_RELATION_NODE = 4;
const unsigned int RECURSE_WAY_NODE = 5;
const unsigned int RECURSE_WAY_RELATION = 6;
const unsigned int RECURSE_NODE_RELATION = 7;
const unsigned int RECURSE_NODE_WAY = 8;

void Recurse_Statement::set_attributes(const char **attr)
{
  map< string, string > attributes;
  
  attributes["from"] = "_";
  attributes["into"] = "_";
  attributes["type"] = "";
  
  eval_cstr_array(get_name(), attributes, attr);
  
  input = attributes["from"];
  output = attributes["into"];
  
  if (attributes["type"] == "relation-relation")
    type = RECURSE_RELATION_RELATION;
  else if (attributes["type"] == "relation-backwards")
    type = RECURSE_RELATION_BACKWARDS;
  else if (attributes["type"] == "relation-way")
    type = RECURSE_RELATION_WAY;
  else if (attributes["type"] == "relation-node")
    type = RECURSE_RELATION_NODE;
  else if (attributes["type"] == "way-node")
    type = RECURSE_WAY_NODE;
  else if (attributes["type"] == "way-relation")
    type = RECURSE_WAY_RELATION;
  else if (attributes["type"] == "node-relation")
    type = RECURSE_NODE_RELATION;
  else if (attributes["type"] == "node-way")
    type = RECURSE_NODE_WAY;
  else
  {
    type = 0;
    ostringstream temp;
    temp<<"For the attribute \"type\" of the element \"recurse\""
	<<" the only allowed values are \"relation-relation\", \"relation-backwards\","
	<<"\"relation-way\", \"relation-node\", \"way-node\", \"way-relation\","
	<<"\"node-relation\" or \"node-way\".";
    add_static_error(temp.str());
  }
}

void Recurse_Statement::forecast()
{
/*  Set_Forecast sf_in(declare_read_set(input));
  Set_Forecast& sf_out(declare_write_set(output));
    
  if (type == RECURSE_RELATION_RELATION)
  {
    sf_out.relation_count = sf_in.relation_count;
    declare_used_time(100*sf_in.relation_count);
  }
  else if (type == RECURSE_RELATION_BACKWARDS)
  {
    sf_out.relation_count = sf_in.relation_count;
    declare_used_time(2000);
  }
  else if (type == RECURSE_RELATION_WAY)
  {
    sf_out.way_count = 22*sf_in.relation_count;
    declare_used_time(100*sf_in.relation_count);
  }
  else if (type == RECURSE_RELATION_NODE)
  {
    sf_out.node_count = 2*sf_in.relation_count;
    declare_used_time(100*sf_in.relation_count);
  }
  else if (type == RECURSE_WAY_NODE)
  {
    sf_out.node_count = 28*sf_in.way_count;
    declare_used_time(50*sf_in.way_count);
  }
  else if (type == RECURSE_WAY_RELATION)
  {
    sf_out.relation_count = sf_in.way_count/10;
    declare_used_time(2000);
  }
  else if (type == RECURSE_NODE_WAY)
  {
    sf_out.way_count = sf_in.node_count/2;
    declare_used_time(sf_in.node_count/1000); //TODO
  }
  else if (type == RECURSE_NODE_RELATION)
  {
    sf_out.relation_count = sf_in.node_count/100;
    declare_used_time(2000);
  }
    
  finish_statement_forecast();
    
  display_full();
  display_state();*/
}

template < class TIndex, class TObject, class TContainer >
void collect_items(const Statement& stmt, Resource_Manager& rman,
		   File_Properties& file_properties,
		   const TContainer& req, vector< uint32 > ids,
		   map< TIndex, vector< TObject > >& result)
{
  uint32 count = 0;
  Block_Backend< TIndex, TObject, typename TContainer::const_iterator > db
      (rman.get_transaction()->data_index(&file_properties));
  for (typename Block_Backend< TIndex, TObject, typename TContainer
      ::const_iterator >::Discrete_Iterator
      it(db.discrete_begin(req.begin(), req.end())); !(it == db.discrete_end()); ++it)
  {
    if (++count >= 64*1024)
    {
      count = 0;
      rman.health_check(stmt);
    }
    if (binary_search(ids.begin(), ids.end(), it.object().id))
      result[it.index()].push_back(it.object());
  }
}

void collect_nodes
    (const Statement& stmt, Resource_Manager& rman,
     map< Uint31_Index, vector< Way_Skeleton > >::const_iterator ways_begin,
     map< Uint31_Index, vector< Way_Skeleton > >::const_iterator ways_end,
     map< Uint32_Index, vector< Node_Skeleton > >& result)
{
  // collect indexes
  vector< Uint32_Index > req;
  {
    vector< uint32 > map_ids;
    vector< uint32 > parents;
    
    for (map< Uint31_Index, vector< Way_Skeleton > >::const_iterator
        it(ways_begin); it != ways_end; ++it)
    {
      if ((it->first.val() & 0x80000000) && ((it->first.val() & 0xf) == 0))
      {
	// Treat ways with really large indices: get the node indexes from nodes.map.
	for (vector< Way_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	{
	  for (vector< uint32 >::const_iterator it3(it2->nds.begin());
	      it3 != it2->nds.end(); ++it3)
	    map_ids.push_back(*it3);
	}
      }
      else
	parents.push_back(it->first.val());
    }
    
    sort(map_ids.begin(), map_ids.end());
    rman.health_check(stmt);
    
    req = calc_node_children(parents);
    
    Random_File< Uint32_Index > random
        (rman.get_transaction()->random_index(osm_base_settings().NODES));
    for (vector< uint32 >::const_iterator
        it(map_ids.begin()); it != map_ids.end(); ++it)
      req.push_back(random.get(*it));
  }    
  rman.health_check(stmt);
  sort(req.begin(), req.end());
  req.erase(unique(req.begin(), req.end()), req.end());
  rman.health_check(stmt);  
  
  // collect ids
  vector< uint32 > ids;
  for (map< Uint31_Index, vector< Way_Skeleton > >::const_iterator
      it(ways_begin); it != ways_end; ++it)
  {
    for (vector< Way_Skeleton >::const_iterator it2(it->second.begin());
        it2 != it->second.end(); ++it2)
    {
      for (vector< uint32 >::const_iterator it3(it2->nds.begin());
	  it3 != it2->nds.end(); ++it3)
	ids.push_back(*it3);
    }
  }
  rman.health_check(stmt);
  sort(ids.begin(), ids.end());
    
  collect_items(stmt, rman, *osm_base_settings().NODES, req, ids, result);
  //stopwatch.add(Stopwatch::NODES, nodes_db.read_count());
}

void Recurse_Statement::execute(Resource_Manager& rman)
{
  stopwatch.start();  
  
  map< string, Set >::const_iterator mit(rman.sets().find(input));
  if (mit == rman.sets().end())
  {
    rman.sets()[output].nodes.clear();
    rman.sets()[output].ways.clear();
    rman.sets()[output].relations.clear();
    rman.sets()[output].areas.clear();
    
    return;
  }

  Set into;
  map< Uint32_Index, vector< Node_Skeleton > >& nodes(into.nodes);
  map< Uint31_Index, vector< Way_Skeleton > >& ways(into.ways);
  map< Uint31_Index, vector< Relation_Skeleton > >& relations(into.relations);

  if (type == RECURSE_RELATION_RELATION)
  {
    set< Uint31_Index > req;
    vector< uint32 > ids;
    
    {
      stopwatch.stop(Stopwatch::NO_DISK);
      Random_File< Uint31_Index > random
          (rman.get_transaction()->random_index(osm_base_settings().RELATIONS));
      for (map< Uint31_Index, vector< Relation_Skeleton > >::const_iterator
	   it(mit->second.relations.begin()); it != mit->second.relations.end(); ++it)
      {
	for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	{
	  for (vector< Relation_Entry >::const_iterator it3(it2->members.begin());
		      it3 != it2->members.end(); ++it3)
	  {
	    if (it3->type == Relation_Entry::RELATION)
	    {
	      req.insert(random.get(it3->ref));
	      ids.push_back(it3->ref);
	    }
	  }
	}
      }
      stopwatch.stop(Stopwatch::RELATIONS_MAP);
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    
    stopwatch.stop(Stopwatch::NO_DISK);
    collect_items(*this, rman, *osm_base_settings().RELATIONS, req, ids, relations);
    //stopwatch.add(Stopwatch::RELATIONS, relations_db.read_count());
    stopwatch.stop(Stopwatch::RELATIONS);
  }
  else if (type == RECURSE_RELATION_BACKWARDS)
  {
    vector< uint32 > ids;
    
    {
      for (map< Uint31_Index, vector< Relation_Skeleton > >::const_iterator
	   it(mit->second.relations.begin()); it != mit->second.relations.end(); ++it)
      {
	for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	  ids.push_back(it2->id);
      }
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    
    stopwatch.stop(Stopwatch::NO_DISK);
    Block_Backend< Uint31_Index, Relation_Skeleton > relations_db
	(rman.get_transaction()->data_index(osm_base_settings().RELATIONS));
    for (Block_Backend< Uint31_Index, Relation_Skeleton >::Flat_Iterator
	 it(relations_db.flat_begin()); !(it == relations_db.flat_end()); ++it)
    {
      const Relation_Skeleton& relation(it.object());
      for (vector< Relation_Entry >::const_iterator it3(relation.members.begin());
          it3 != relation.members.end(); ++it3)
      {
	if ((it3->type == Relation_Entry::RELATION) &&
	    (binary_search(ids.begin(), ids.end(), it3->ref)))
	{
	  relations[it.index()].push_back(relation);
	  break;
	}
      }
    }
    stopwatch.add(Stopwatch::RELATIONS, relations_db.read_count());
    stopwatch.stop(Stopwatch::RELATIONS);
  }
  else if (type == RECURSE_RELATION_WAY)
  {
    vector< Uint31_Index > req;
    vector< uint32 > ids;    
    {
      vector< uint32 > map_ids;
      vector< uint32 > parents;
      
      for (map< Uint31_Index, vector< Relation_Skeleton > >::const_iterator
	   it(mit->second.relations.begin()); it != mit->second.relations.end(); ++it)
      {
	if ((it->first.val() & 0x80000000) && ((it->first.val() & 0xf) == 0))
	{
	  // Treat ways with really large indices: get the way indexes from ways.map.
	  for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	      it2 != it->second.end(); ++it2)
	  {
	    for (vector< Relation_Entry >::const_iterator it3(it2->members.begin());
		      it3 != it2->members.end(); ++it3)
	    {
	      if (it3->type == Relation_Entry::WAY)
	        map_ids.push_back(it3->ref);
	    }
	  }
	}
	else
	  parents.push_back(it->first.val());

	for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	{
	  for (vector< Relation_Entry >::const_iterator it3(it2->members.begin());
	      it3 != it2->members.end(); ++it3)
	  {
	    if (it3->type == Relation_Entry::WAY)
	      ids.push_back(it3->ref);
	  }
	}
      }
      vector< uint32 > children = calc_children(parents);
      req.reserve(children.size());
      for (vector< uint32 >::const_iterator it = children.begin(); it != children.end(); ++it)
	req.push_back(Uint31_Index(*it));
      
      sort(map_ids.begin(), map_ids.end());

      stopwatch.stop(Stopwatch::NO_DISK);
      rman.health_check(*this);
      
      Random_File< Uint31_Index > random
          (rman.get_transaction()->random_index(osm_base_settings().WAYS));
      for (vector< uint32 >::const_iterator
	  it(map_ids.begin()); it != map_ids.end(); ++it)
	req.push_back(random.get(*it));
      stopwatch.stop(Stopwatch::WAYS_MAP);
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    
    rman.health_check(*this);
    sort(req.begin(), req.end());
    req.erase(unique(req.begin(), req.end()), req.end());
    rman.health_check(*this);
    stopwatch.stop(Stopwatch::NO_DISK);
    
    collect_items(*this, rman, *osm_base_settings().WAYS, req, ids, ways);
    //stopwatch.add(Stopwatch::WAYS, ways_db.read_count());
    stopwatch.stop(Stopwatch::WAYS);
  }
  else if (type == RECURSE_RELATION_NODE)
  {
    vector< Uint32_Index > req;
    vector< uint32 > ids;    
    {
      vector< uint32 > map_ids;
      vector< uint32 > parents;
      
      for (map< Uint31_Index, vector< Relation_Skeleton > >::const_iterator
	   it(mit->second.relations.begin()); it != mit->second.relations.end(); ++it)
      {
	if ((it->first.val() & 0x80000000) && ((it->first.val() & 0xf) == 0))
	{
	  // Treat ways with really large indices: get the node indexes from nodes.map.
	  for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	      it2 != it->second.end(); ++it2)
	  {
	    for (vector< Relation_Entry >::const_iterator it3(it2->members.begin());
		      it3 != it2->members.end(); ++it3)
	    {
	      if (it3->type == Relation_Entry::NODE)
	        map_ids.push_back(it3->ref);
	    }
	  }
	}
	else
	  parents.push_back(it->first.val());

	for (vector< Relation_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	{
	  for (vector< Relation_Entry >::const_iterator it3(it2->members.begin());
	      it3 != it2->members.end(); ++it3)
	  {
	    if (it3->type == Relation_Entry::NODE)
	      ids.push_back(it3->ref);
	  }
	}
      }
      
      sort(map_ids.begin(), map_ids.end());

      stopwatch.stop(Stopwatch::NO_DISK);
      rman.health_check(*this);
      
      req = calc_node_children(parents);
      
      Random_File< Uint32_Index > random
          (rman.get_transaction()->random_index(osm_base_settings().NODES));
      for (vector< uint32 >::const_iterator
	  it(map_ids.begin()); it != map_ids.end(); ++it)
	req.push_back(random.get(*it));
      stopwatch.stop(Stopwatch::NODES_MAP);
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    
    rman.health_check(*this);
    sort(req.begin(), req.end());
    req.erase(unique(req.begin(), req.end()), req.end());
    rman.health_check(*this);
    stopwatch.stop(Stopwatch::NO_DISK);
    
    collect_items(*this, rman, *osm_base_settings().NODES, req, ids, nodes);
    //stopwatch.add(Stopwatch::NODES, nodes_db.read_count());
    stopwatch.stop(Stopwatch::NODES);
  }
  else if (type == RECURSE_WAY_NODE)
    collect_nodes(*this, rman, mit->second.ways.begin(), mit->second.ways.end(), nodes);
  else if (type == RECURSE_WAY_RELATION)
  {
    set< Uint31_Index > req;
    vector< uint32 > children;
    vector< uint32 > ids;
    
    {
      for (map< Uint31_Index, vector< Way_Skeleton > >::const_iterator
	   it(mit->second.ways.begin()); it != mit->second.ways.end(); ++it)
      {
	children.push_back(it->first.val());
	for (vector< Way_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	  ids.push_back(it2->id);
      }
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    vector< uint32 > parents = calc_parents(children);
    for (vector< uint32 >::const_iterator it = parents.begin(); it != parents.end(); ++it)
      req.insert(Uint31_Index(*it));
    
    stopwatch.stop(Stopwatch::NO_DISK);
    Block_Backend< Uint31_Index, Relation_Skeleton > relations_db
	(rman.get_transaction()->data_index(osm_base_settings().RELATIONS));
    for (Block_Backend< Uint31_Index, Relation_Skeleton >::Discrete_Iterator
	 it(relations_db.discrete_begin(req.begin(), req.end()));
	 !(it == relations_db.discrete_end()); ++it)
    {
      const Relation_Skeleton& relation(it.object());
      for (vector< Relation_Entry >::const_iterator it3(relation.members.begin());
          it3 != relation.members.end(); ++it3)
      {
	if ((it3->type == Relation_Entry::WAY) &&
	    (binary_search(ids.begin(), ids.end(), it3->ref)))
	{
	  relations[it.index()].push_back(relation);
	  break;
	}
      }
    }
    stopwatch.add(Stopwatch::RELATIONS, relations_db.read_count());
    stopwatch.stop(Stopwatch::RELATIONS);
  }
  else if (type == RECURSE_NODE_WAY)
  {
    set< Uint31_Index > req;
    vector< uint32 > children;
    vector< uint32 > ids;
    
    {
      for (map< Uint32_Index, vector< Node_Skeleton > >::const_iterator
	   it(mit->second.nodes.begin()); it != mit->second.nodes.end(); ++it)
      {
	children.push_back(it->first.val());
	for (vector< Node_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	  ids.push_back(it2->id);
      }
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    vector< uint32 > parents = calc_parents(children);
    for (vector< uint32 >::const_iterator it = parents.begin(); it != parents.end(); ++it)
      req.insert(Uint31_Index(*it));
    
    stopwatch.stop(Stopwatch::NO_DISK);
    Block_Backend< Uint31_Index, Way_Skeleton > ways_db
	(rman.get_transaction()->data_index(osm_base_settings().WAYS));
    for (Block_Backend< Uint31_Index, Way_Skeleton >::Discrete_Iterator
	 it(ways_db.discrete_begin(req.begin(), req.end()));
	 !(it == ways_db.discrete_end()); ++it)
    {
      const Way_Skeleton& way(it.object());
      for (vector< uint32 >::const_iterator it3(way.nds.begin());
          it3 != way.nds.end(); ++it3)
      {
        if (binary_search(ids.begin(), ids.end(), *it3))
	{
	  ways[it.index()].push_back(way);
	  break;
	}
      }
    }
    stopwatch.add(Stopwatch::WAYS, ways_db.read_count());
    stopwatch.stop(Stopwatch::WAYS);
  }
  else if (type == RECURSE_NODE_RELATION)
  {
    set< Uint31_Index > req;
    vector< uint32 > children;
    vector< uint32 > ids;
    
    {
      for (map< Uint32_Index, vector< Node_Skeleton > >::const_iterator
	   it(mit->second.nodes.begin()); it != mit->second.nodes.end(); ++it)
      {
	children.push_back(it->first.val());
	for (vector< Node_Skeleton >::const_iterator it2(it->second.begin());
	    it2 != it->second.end(); ++it2)
	  ids.push_back(it2->id);
      }
    }
    rman.health_check(*this);
    sort(ids.begin(), ids.end());
    vector< uint32 > parents = calc_parents(children);
    for (vector< uint32 >::const_iterator it = parents.begin(); it != parents.end(); ++it)
      req.insert(Uint31_Index(*it));
    
    stopwatch.stop(Stopwatch::NO_DISK);
    Block_Backend< Uint31_Index, Relation_Skeleton > relations_db
	(rman.get_transaction()->data_index(osm_base_settings().RELATIONS));
    for (Block_Backend< Uint31_Index, Relation_Skeleton >::Discrete_Iterator
	 it(relations_db.discrete_begin(req.begin(), req.end()));
	 !(it == relations_db.discrete_end()); ++it)
    {
      const Relation_Skeleton& relation(it.object());
      for (vector< Relation_Entry >::const_iterator it3(relation.members.begin());
          it3 != relation.members.end(); ++it3)
      {
	if ((it3->type == Relation_Entry::NODE) &&
	    (binary_search(ids.begin(), ids.end(), it3->ref)))
	{
	  relations[it.index()].push_back(relation);
	  break;
	}
      }
    }
    stopwatch.add(Stopwatch::RELATIONS, relations_db.read_count());
    stopwatch.stop(Stopwatch::RELATIONS);
  }

  into.nodes.swap(rman.sets()[output].nodes);
  into.ways.swap(rman.sets()[output].ways);
  into.relations.swap(rman.sets()[output].relations);
  rman.sets()[output].areas.clear();
  
  stopwatch.stop(Stopwatch::NO_DISK);
  stopwatch.report(get_name());
  rman.health_check(*this);
}
