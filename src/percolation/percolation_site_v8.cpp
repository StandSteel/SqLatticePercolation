//
// Created by shahnoor on 12/27/2017.
//


#include <cstdlib>
#include <climits>
#include <unordered_set>
#include <mutex>

#include "percolation.h"

#include "../util/printer.h"
#include "site_position.h"
#include "../lattice/connector.h"
#include "../index/delta.h"
#include <omp.h>
#include <thread>

using namespace std;

/**
 *  variables
 */
double total_time_for_find_index_for_placing_new_bonds_7{};
double time_7_relabel_sites{};
value_type time_7_total_relabeling_N{};
double time_7_relabel_sites_old{};
value_type time_7_total_relabeling_N_old{};
value_type time_7_put_values_to_the_cluster_N{};
double time_7_put_values_to_the_cluster{};
double time_7_put_values_to_the_cluster_weighted_relabeling{};
value_type time_7_put_values_to_the_cluster_weighted_relabeling_N{};
double time_7_relabelMap{};
double time_7_connection2{};



/**
 *
 * @param length       : length of the lattice
 * @param impure_sites : number of impure sites. cannot be greater than length*length
 */
SitePercolation_ps_v8::SitePercolation_ps_v8(value_type length, bool periodicity)
    :SqLatticePercolation(length)
{
    std::cout << "Constructing SitePercolation_ps_v8 object : line " << __LINE__ << endl;
    SqLatticePercolation::set_type('s');
//    if(impure_sites > length*length){
//        cout << "Too many impure sites : line " << __LINE__ << endl;
//    }
//    _impure_sites = impure_sites;
    _periodicity = periodicity;
    _index_sequence_position = 0;
    _lattice = SqLattice(length, true, false, false, true);   // since it is a site percolation all bonds will be activated by default

    min_index = 0;
    max_index = length - 1;

    randomized_index_sequence = vector<Index>(maxSites());
    index_sequence = vector<Index>(maxSites());
    _max_iteration_limit = maxSites();

    initialize_index_sequence();
    initialize();
    randomize();  // randomize the untouched_site_indices
//    markImpureSites();
}


/**
 *
 */
void SitePercolation_ps_v8::initialize() {

    // to improve performence
    number_of_sites_to_span.reserve(maxSites());
    number_of_bonds_to_span.reserve(maxSites());

    _top_edge.reserve(length());
    _bottom_edge.reserve(length());
    _left_edge.reserve(length());
    _right_edge.reserve(length());

    randomized_index_sequence = index_sequence;
}


/**
 * Called only once when the object is constructed for the first time
 */
void SitePercolation_ps_v8::initialize_index_sequence() {
    value_type m{}, n{};
    for (value_type i{}; i != index_sequence.size(); ++i) {
        index_sequence[i] = Index(m, n);
        ++n;
        if (n == length()) {
            n = 0;
            ++m;
        }
    }
    //for (value_type i{}; i != index_sequence.size(); ++i) {cout << index_sequence[i] << endl;}
}


/**
 * Reset all calculated values and then call initiate()
 * to initiallize for reuse
 *
 * caution -> it does not erase _calculation_flags, for it will be used for calculation purposes
 */
void SitePercolation_ps_v8::reset() {
    SqLatticePercolation::reset();
    // variables
    _number_of_occupied_sites = 0;
    _index_sequence_position = 0;
//    _first_spanning_cluster_id = -1;
    _cluster_id = 0;
//    bonds_in_largest_cluster = 0;
//    sites_in_largest_cluster = 0;

    // containers
//    randomized_index_sequence.clear();    // reseted in the initialize function

//    _number_of_occupied_sites.clear();
//    _entropy_by_bond.clear();
    number_of_sites_to_span.clear();
    number_of_bonds_to_span.clear();
//    spanning_cluster_ids.clear();
    _spanning_sites.clear();
    _wrapping_sites.clear();
//    wrapping_cluster_ids.clear();

    _cluster_entropy.clear();
    _bonds_in_cluster_with_size_two_or_more = 0;
//    _id_largest_cluster = 0;

//    _id_last_modified_cluster = -1;

    _index_last_modified_cluster = 0;  // id of the last modified cluster
//    _index_largest_cluster = 0;
    _number_of_bonds_in_the_largest_cluster = 0;
    _number_of_sites_in_the_largest_cluster = 0;
//    _cluster_id_set.clear();

    _entropy_by_bond = 0;
    _entropy_by_bond_to_add = 0;
    _entropy_by_bond_to_subtract = 0;
    _entropy_by_site =0;
    _entropy_by_site_to_add=0;
    _entropy_by_site_to_subtract = 0;
    _entropy_by_site_would_be = 0;

    // clearing edges
    _top_edge.clear();
    _bottom_edge.clear();
    _left_edge.clear();
    _right_edge.clear();

    _spanning_occured = false;

    initialize();
    randomize();
//    markImpureSites();
    time_relabel = 0;
   _total_relabeling = 0;
}


/**
 * Randomize the site placing order
 * Takes 3.031000 sec for 1000 x 1000 sites
 */
void SitePercolation_ps_v8::randomize(){
    value_type  len = randomized_index_sequence.size();
    value_type j{};
    Index temp;
    for(value_type i{} ; i != len; ++i){
        // select a j from the array. which must come from the ordered region
        j = i + std::rand() % (len - i);

        // perform the swapping with i-th and j-th value
        temp = randomized_index_sequence[j];
        randomized_index_sequence[j] = randomized_index_sequence[i];
        randomized_index_sequence[i] = temp;
    }
//    cout << "Index sequence : " << randomized_index_sequence << endl;
}

/**
 * Impure atom id is set to -2
 */
//void SitePercolation_ps_v8::markImpureSites() {
//    while(_index_sequence_position < _impure_sites){
//        _lattice.getSite(randomized_index_sequence[_index_sequence_position]).set_groupID(_impurity_id);
//        ++_index_sequence_position;
//    }
//}

/**
 * Create a custome configuration for the lattice
 */
void SitePercolation_ps_v8::configure(std::vector<Index> site_indices) {
    cout << "Entry -> configure() : line " << endl;
    if (_number_of_occupied_sites > 0) {
        cout << "The lattice is not in the initial state" << endl;
        cout << "setting _number_of_occupied_sites = 0" << endl;
        _number_of_occupied_sites = 0;
    }

    // mark active sites
    randomized_index_sequence = site_indices;
//    randomize();
    while (_number_of_occupied_sites < site_indices.size()){
        placeSite_v6();
        viewClusterExtended();
        viewLatticeExtended();
    }
//    std::vector<value_type> site_index_sequence(site_indices.size());
//    for (value_type i{}; i != site_index_sequence.size(); ++i) {
//        site_index_sequence[i] = i;
//    }
//
//    value_type x;
//    Index s_index;
//    value_type count{};
//    while (!site_index_sequence.empty()) {
//        ++count;
//        x = std::rand() % site_index_sequence.size();
//        s_index = site_indices[site_index_sequence[x]];
//        site_index_sequence.erase(site_index_sequence.begin() + x); // must erase used site_index_sequence value
//
//        // checking if there is an invalid index;
//        if (s_index.x_ >= length() || s_index.y_ >= length()) {
//            cerr << "Invalid index : line " << __LINE__ << endl;
//            cerr << "site_indices[i].x_ >= length() || site_indices[i].y_ >= length()  : line " << __LINE__ << endl;
//            return;
//        }
//        _lattice.getSite(s_index).activate();
//        ++_number_of_occupied_sites;
//
//        // find the bonds for this site
//        vector<Bond> hv_bonds;
//        if(_periodicity) {
//            hv_bonds = get_Bond_for_site(s_index);// old
//        }
//        else {
//            hv_bonds = get_Bond_for_site_no_periodicity(s_index); // new
//        }
//
//        // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
//        set<value_type> found_index_set = find_index_for_placing_new_bonds_v1(hv_bonds);
////        value_type m = put_values_to_the_cluster_weighted_relabeling_v1(found_index_set, hv_bonds, s_index);
////        set<value_type> found_index_set = find_index_for_placing_new_bonds_v2(s_index);
//        value_type m = put_values_to_the_cluster_weighted_relabeling_v2(found_index_set, hv_bonds, s_index);
//
//        cout << "_cluster_index_from_id : \n" << _cluster_index_from_id << endl;
//        if (debug_4_configure) {
//            cout << "********************** view ***************** : line " << __LINE__ << endl;
//            viewLatticeExtended();
//            viewClusterExtended();
//        }
//    }


}


/*************************************************
 * Calculation methods
 *
 ***********************************/

/*
 * Instead of calculating entropy for 1000s of cluster in every iteration
 * just keep track of entropy change, i.e.,
 * how much to subtract and how much to add.
 */
/**
 * Must be called before merging the clusters
 * @param found_index_set
 */
void SitePercolation_ps_v8::subtract_entropy_for_bond(const set<value_type> &found_index){
    double nob, mu_bond;
    for(auto x : found_index){
        nob = _clusters[x].numberOfBonds();
        mu_bond = nob/ maxBonds();
        _entropy_by_bond_to_subtract += -log(mu_bond) * mu_bond;
    }
}

void SitePercolation_ps_v8::subtract_entropy_for_bond(const vector<value_type> &found_index){
    double nob, mu_bond;
    for(auto x : found_index){
        nob = _clusters[x].numberOfBonds();
        mu_bond = nob/ maxBonds();
        _entropy_by_bond_to_subtract += -log(mu_bond) * mu_bond;
    }
}


void SitePercolation_ps_v8::subtract_entropy_for_site(const vector<value_type>& found_index){
    double nos, mu_site;
    for(auto x : found_index){
        nos = _clusters[x].numberOfSites();
        mu_site = nos/_number_of_occupied_sites;
        _entropy_by_site_to_subtract += -log(mu_site) * mu_site;
    }
}


void SitePercolation_ps_v8::subtract_entropy_for_site(const set<value_type>& found_index){
    double nos, mu_site;
    for(auto x : found_index){
        nos = _clusters[x].numberOfSites();
        mu_site = nos/_number_of_occupied_sites;
        _entropy_by_site_to_subtract += -log(mu_site) * mu_site;
    }
}

void SitePercolation_ps_v8::subtract_entropy_for_full(const vector<value_type>& found_index){
    double nob, mu_bond, nos, mu_site;
    for(auto x : found_index){
        nob = _clusters[x].numberOfBonds();
        nos = _clusters[x].numberOfSites();
        mu_bond = nob/ maxBonds();
        mu_site = nos/_number_of_occupied_sites;

        _entropy_by_bond_to_subtract += -log(mu_bond) * mu_bond;
        _entropy_by_site_to_subtract += -log(mu_site) * mu_site;
    }
}


void SitePercolation_ps_v8::subtract_entropy_for_full(const set<value_type>& found_index){
    double nob, mu_bond, nos, mu_site;
    for(auto x : found_index){
        nob = _clusters[x].numberOfBonds();
        nos = _clusters[x].numberOfSites();
        mu_bond = nob/ maxBonds();
        mu_site = nos/_number_of_occupied_sites;

        _entropy_by_bond_to_subtract += -log(mu_bond) * mu_bond;
        _entropy_by_site_to_subtract += -log(mu_site) * mu_site;
    }
}


/**
 * Must be called after merging the clusters
 * Cluster length is measured by bonds
 * @param index
 */
void SitePercolation_ps_v8::add_entropy_for_bond(value_type index){
    double nob = _clusters[index].numberOfBonds();
    double mu_bond = nob / maxBonds();
    _entropy_by_bond_to_add = -log(mu_bond) * mu_bond;
    _entropy_by_bond = _entropy_by_bond + _entropy_by_bond_to_add - _entropy_by_bond_to_subtract;   // keeps track of entropy
    _entropy_by_bond_to_subtract = 0;
    _entropy_by_bond_to_add = 0;
}


/**
 * Must be called after merging the clusters
 * Cluster length is measured by sites
 * @param index
 */
void SitePercolation_ps_v8::add_entropy_for_site(value_type index){
    double nos = _clusters[index].numberOfSites();
    double mu = nos / _number_of_occupied_sites;

    _entropy_by_site_to_add = -log(mu) * mu;
    _entropy_by_site = _entropy_by_site + _entropy_by_site_to_add - _entropy_by_site_to_subtract;   // keeps track of entropy
    _entropy_by_site_to_subtract = 0;
    _entropy_by_site_to_add = 0;
}

/**
 * Must be called after merging the clusters
 * Cluster length is measured by sites
 * @param index
 */
void SitePercolation_ps_v8::add_entropy_for_full(value_type index){
    double nos = _clusters[index].numberOfSites();
    double mu_site = nos / _number_of_occupied_sites;
//    cout << "mu_site " << mu_site << endl;
    _entropy_by_site_to_add = -log(mu_site) * mu_site;
    _entropy_by_site += _entropy_by_site_to_add - _entropy_by_site_to_subtract;   // keeps track of entropy
    _entropy_by_site_to_subtract = 0;
    _entropy_by_site_to_add = 0;
    // would be
//    double mu_site_would_be = nos / (_number_of_occupied_sites + 1); // in the next iteration
//    _entropy_by_site_would_be += -log(mu_site_would_be) * mu_site_would_be;

    // by bond
    double nob = _clusters[index].numberOfBonds();
    double mu_bond = nob / maxBonds();
    _entropy_by_bond_to_add = -log(mu_bond) * mu_bond;
    _entropy_by_bond += _entropy_by_bond_to_add - _entropy_by_bond_to_subtract;   // keeps track of entropy
    _entropy_by_bond_to_subtract = 0;
    _entropy_by_bond_to_add = 0;
}




/**
 * Condition: must be called each time a site is placed
 */
void SitePercolation_ps_v8::track_numberOfBondsInLargestCluster() {

    // calculating number of bonds in the largest cluster // by cluster index
    // checking number of bonds
    if(_clusters[_index_last_modified_cluster].numberOfBonds() > _number_of_bonds_in_the_largest_cluster){
        _number_of_bonds_in_the_largest_cluster = _clusters[_index_last_modified_cluster].numberOfBonds();
    }

}

/**
 *
 */
void SitePercolation_ps_v8::track_numberOfSitesInLargestCluster(){

    // calculating number of bonds in the largest cluster // by cluster index
    // checking number of bonds
    if(_clusters[_index_last_modified_cluster].numberOfSites() > _number_of_sites_in_the_largest_cluster){
        _number_of_sites_in_the_largest_cluster = _clusters[_index_last_modified_cluster].numberOfSites();
    }
}

/**
 * Condition: must be called each time a site is placed
 */
void SitePercolation_ps_v8::track_entropy() {
    double mu = _clusters[_index_last_modified_cluster].numberOfBonds() / double(maxBonds());
    _cluster_entropy[_clusters[_index_last_modified_cluster].get_ID()] = mu * log(mu);
}


/**
 * Takes 0.093 sec time when total time is 9.562 sec
 *
 * Find one row from _cluster to place 4 or less new bonds
 * Also remove the matched index values, because they will be inserted later.
 * This gives an advantage, i.e., you don't need to perform a checking.
 * todo takes so much time
 * @param hv_bonds
 * @return a set
 */
set<value_type>
SitePercolation_ps_v8::find_index_for_placing_new_bonds_v3(const vector<Index>& neighbors) {
    clock_t t = clock();              // for Time calculation purposes only
    value_type total_found_bonds{}; // for checking program running Time only

    set<value_type> found_index_set;    // use set to prevent repeated index
    value_type x{};

    if(!_cluster_index_from_id.empty()) {
        for (auto n: neighbors) {
            if(_cluster_index_from_id.count(_lattice.getSite(n).get_groupID()) > 0) {

                x = _cluster_index_from_id[_lattice.getSite(n).get_groupID()];
                found_index_set.insert(x);

            }
        }
    }


    return found_index_set;
}

/**
 * Find cluster of size greater than 1 that includes neighbors.
 * Also finds the suitable base cluster to get weighted relabeling.
 * @param neighbors       : std::vector<value_type>&,
 *                          index of the neighbor sites
 * @param found_index_set : std::set<value_type>&,
 *                          index of the cluster of neighbors which belongs
 *                          to a cluster of size greater thatn 1
 * @return                : value type,
 *                          suitable base cluster index
 */
value_type 
SitePercolation_ps_v8::find_index_for_placing_new_bonds_v4(const vector<Index>& neighbors, set<value_type>& found_index_set) {
    clock_t t = clock();              // for Time calculation purposes only
    value_type total_found_bonds{}; // for checking program running Time only

    found_index_set.clear();
    value_type x{}, base{}, tmp{}, sz{};
    bool first{true};
    int id{};
    if(!_cluster_index_from_id.empty()) {
        for (auto n: neighbors) {
            id = _lattice.getSite(n).get_groupID();
            if(_cluster_index_from_id.count(id) > 0) {

                x = _cluster_index_from_id[id];
                if(first) {
                    base = x;
                    sz = _clusters[x].numberOfSites();
                    first = false;
                }else{
                    tmp = _clusters[x].numberOfSites();
                    if(tmp > sz){
//                        cout << "cluster [" << x << "] ID " << id << " size " << tmp << " > " << sz << endl;
                        sz = tmp;
                        base = x;
//                        continue; // since it is base it's not included in the found_index_set // todo check if it works
                    }
                }
                found_index_set.insert(x);

            }
        }
    }

//    cout << "found base is " << base << endl;
    return base;
}


bool ispresent(const std::vector<value_type> &v, value_type a){
    for(auto x: v){
        if(x == a){
            return true;
        }
    }
    return false;
}

/**
 * Find one row from _cluster to place 4 or less new bonds
 * Also remove the matched index values, because they will be inserted later.
 * This gives an advantage, i.e., you don't need to perform a checking.
 * todo takes so much time
 *
 *
 * @param hv_bonds
 * @return a vector of indices sorted with respect to the number of sites of clusters
 */
vector<value_type>
SitePercolation_ps_v8::find_index_for_placing_new_bonds_v4(vector<Index> neighbors) {
    clock_t t = clock();              // for Time calculation purposes only
    value_type total_found_bonds{}; // for checking program running Time only

    vector<value_type> found_index;    // use set to prevent repeated index
    value_type x{}, tmp{};

    if(!_cluster_index_from_id.empty()) {
        for (auto n: neighbors) {
            if(_cluster_index_from_id.count(_lattice.getSite(n).get_groupID()) > 0) {
                x = _cluster_index_from_id[_lattice.getSite(n).get_groupID()];
                if(!ispresent(found_index, x)) { // if x is not in the array then store it
                    found_index.push_back(x);
                    if (found_index.size() > 1) {
                        // perform the sorting
                        if (_clusters[found_index[0]].numberOfSites() < _clusters[found_index[1]].numberOfSites()) {
                            // swap the indices to sort according to the cluster size
                            tmp = found_index[1];
                            found_index[1] = found_index[0];
                            found_index[0] = tmp;
                        }
                    }
                }
            }
        }
    }



    return found_index;
}




/**
 * When merging two cluster this function perform relabeling based
 * on the gived indices. If the index_1 is smaller then relabel
 * sites that belong to index_1 using index_2 then return index_1
 *
 * @param index_1
 * @param index_2
 * @return  : index of the cluster whose members are relabeled and this cluster must be erased
 */
value_type SitePercolation_ps_v8::relabel(value_type index_1, value_type index_2){
    if(_clusters[index_1].numberOfSites() < _clusters[index_2].numberOfSites()){
        // _clusters[index_1] is smaller

        relabel_sites(_clusters[index_1], _clusters[index_2].get_ID()); // for site percolation
//        relabel_bonds(_clusters[index_1], _clusters[index_2].set_ID()); // for bond percolation
        return index_1;
    }else{
        relabel_sites(_clusters[index_2], _clusters[index_1].get_ID()); // for site percolation
//        relabel_bonds(_clusters[index_2], _clusters[index_1].set_ID()); // for bond percolation
        return index_2;
    }
}



/**
 * Relative index of site_new with respect to root
 * @param root
 * @param site_new
 * @return
 */
IndexRelative SitePercolation_ps_v8::getRelativeIndex(Index root, Index site_new){
//    cout << "Entry \"SitePercolation_ps_v8::getRelativeIndex\" : line " << __LINE__ << endl;
    int delta_x = -int(root.column_) + int(site_new.column_); // if +1 then root is on the right ??
    int delta_y = int(root.row_) - int(site_new.row_); // if +1 then root is on the top ??


    // normalizing delta_x
    if(delta_x > 1){
        delta_x /= -delta_x;
    }
    else if(delta_x < -1){
        delta_x /= delta_x;
    }

    // normalizing delta_y
    if(delta_y > 1){
        delta_y /= -delta_y;
    }else if(delta_y < -1){
        delta_y /= delta_y;
    }

    IndexRelative indexRelative_root = _lattice.getSite(root).relativeIndex();
//    cout << "Relative index of root " << indexRelative_root << endl;
//    cout << "Delta x,y " << delta_x << ", " << delta_y << endl;
    IndexRelative r =  {indexRelative_root.x_ + delta_x, indexRelative_root.y_ + delta_y};
//    cout << "Relative index of site_new " << r << endl;
    return r;
}

/**
 *
 * @param found_index_set
 * @param hv_bonds
 * @param site
 * @return
 */
value_type SitePercolation_ps_v8::manage_clusters_v10(
        const set<value_type> &found_index_set,
        vector<BondIndex> &hv_bonds,
        Index &site
)
{

    vector<value_type> found_index(found_index_set.begin(), found_index_set.end());
    value_type merged_cluster_index{};

    if (!found_index_set.empty()) {
        unsigned long base = found_index[0];
        Index root = _clusters[base].getRootSite(); // root index of the base cluster
        int id_base = _clusters[base].get_ID();

        _clusters[base].addSiteIndex(site);

        vector<Index> neibhgors = _lattice.get_neighbor_site_indices(site);
        // find which of the neighbors are of id as the base cluster
        IndexRelative r;
        for(auto n: neibhgors){
            if(_lattice.getSite(n).get_groupID() == id_base){
                // find relative index with respect to this site
                r = getRelativeIndex(n, site);
                break; // since first time r is set tunning loop is doing no good
            }
        }

        _lattice.getSite(site).relativeIndex(r);
        _lattice.getSite(site).set_groupID(id_base); // relabeling for 1 site

        // put_values_to_the_cluster new values in the 0-th found index
        _clusters[base].insert(hv_bonds);
        // merge clusters with common values from all other cluster
        int tmp_id;
        for (value_type k{1}; k != found_index.size(); ++k) {

            unsigned long ers = found_index[k];
            _total_relabeling += _clusters[ers].numberOfSites();

            tmp_id = id_base;

            // erase the redundant cluster ids

            int id_to_be_deleted = _clusters[ers].get_ID();
            _cluster_index_from_id.erase(id_to_be_deleted); // first erase previous keys

            // perform relabeling on the sites
            relabel_sites_v5(site, _clusters[ers]);

            // since we use cluster id to relabel cluster when merging, cluster also need to be updated
            _clusters[ers].set_ID(tmp_id);

            /// since all cluster will eventually get merged to cluster with
            /// index found_index[0] whatever the id is index must be found_index[0]

            // store values of other found indices to the cluster
            _clusters[base].insert(_clusters[ers]);


            // delete the merged cluster
            auto it = _clusters.begin() + ers;
            _clusters.erase(it);
            // after erasing found_index cannot remain same. come on.
            // If you don't do this we will be deleting wrond clusters
            for (value_type m{k + 1}; m != found_index.size(); ++m) {
                // only the values of found_index that will be used next Time must be reduced
                --found_index[m];
            }
//            cout << "merging cluster completed : line " << __LINE__ << endl;
        }
        merged_cluster_index = base;

    } else {
        // create new element for the cluster
        _clusters.push_back(Cluster_v2(_cluster_id));
        merged_cluster_index = _clusters.size() -1;  // this new cluster index
//        _cluster_index_from_id[_cluster_id] = _clusters.size() - 1; // keeps track of cluster id and cluster index
        _cluster_index_from_id.insert(_cluster_id); // new version
        _lattice.getSite(site).set_groupID(_cluster_id); // relabeling for 1 site
        _cluster_id++;  // increase the cluster id for next round
        _clusters.back().insert(hv_bonds);
        _clusters[merged_cluster_index].addSiteIndex(site);

    }


    // data for short cut calculation
    _index_last_modified_cluster = merged_cluster_index;
    _bonds_in_cluster_with_size_two_or_more += hv_bonds.size();

    return merged_cluster_index;
}


/**
 * Out of all possible index in  found_index this function finds the index of the largest cluster,
 * which will enable the program to do weighted relabeling.
 * @param found_index
 * @return
 */
value_type SitePercolation_ps_v8::find_suitable_base_cluster(const vector<value_type> &found_index) const {
    value_type base = 0;
//    cout << "Finding base : line " << __LINE__ << endl;
    if (!found_index.empty()){
        // first find the base cluster, usually the largest cluster
        base = found_index[0];
        value_type sz = _clusters[base].numberOfSites(), tmp;

//        cout << "cluster [" << base << "] ID " << _clusters[base].get_ID() << " size " << sz << endl;

        for(value_type k{1}; k < found_index.size(); ++k){
            tmp = _clusters[found_index[k]].numberOfSites();
//            cout << "cluster [" << found_index[k] << "] ID " << _clusters[found_index[k]].get_ID() << " size " << tmp << endl;
            if(tmp > sz){
                sz = tmp;
                base = found_index[k];
            }
        }
//        cout << "found base " << base << endl;
        return base;
    }else {
        // do not comment this line. if you see this line, it means code has some serious issues.
        cerr << "No base should be found : line " << __LINE__ << endl;
        exit(1);
        //return base;
    }
}

/**
 *
 */
void SitePercolation_ps_v8::check(Index current_site){
//    for(auto x: _cluster_index_from_id){
//        if(x.second >= _clusters.size()){
//            viewClusterExtended();
//            viewLatticeExtended();
//            cout << "Invalid index when placing site " << current_site << endl;
//            cerr << _cluster_index_from_id << endl;
//            cerr << x.first << "->" << x.second << endl;
//            throw InvalidIndex{"invalid index at line " + to_string(__LINE__)};
//        }
//        // if no exception is thrown. check if id and index matches each other
//        if(_clusters[x.second].set_ID() != x.first){
//            viewClusterExtended();
//            viewLatticeExtended();
//            cout << "Id and index mismatch when placing " << current_site << endl;
//            cerr << _cluster_index_from_id << endl;
//            cerr << _clusters[x.second].set_ID() << "!=" <<  x.first << endl;
//            throw Mismatch{__LINE__, to_string(x.first) + "->" + to_string(x.second)};
//        }
//    }
}



/**
 * Take a bond index only if the corresponding site is active
 * lengthy but straight forward
 * @param site
 * @param neighbors
 * @param bonds
 */
void SitePercolation_ps_v8::connection_v1(Index site, vector<Index> &neighbors, vector<BondIndex> &bonds)
{
    clock_t t = clock();
    value_type prev_column  = (site.column_ + length() - 1) % length();
    value_type prev_row     = (site.row_ + length() - 1) % length();
    value_type next_row     = (site.row_ + 1) % length();
    value_type next_column  = (site.column_ + 1) % length();
    if(_periodicity){
        neighbors.resize(4);
        neighbors[0] = {site.row_, next_column};
        neighbors[1] = {site.row_, prev_column};
        neighbors[2] = {next_row, site.column_};
        neighbors[3] = {prev_row, site.column_};

        bonds.reserve(4);
        if(!_lattice.getSite(neighbors[0]).isActive()) {
            bonds.push_back({BondType::Horizontal, site.row_, site.column_});
        }
        if(!_lattice.getSite(neighbors[1]).isActive()){
            bonds.push_back({BondType::Horizontal, site.row_, prev_column});
        }
        if(!_lattice.getSite(neighbors[2]).isActive()){
            bonds.push_back({BondType::Vertical,    site.row_, site.column_});
        }
        if(!_lattice.getSite(neighbors[3]).isActive()){
            bonds.push_back({BondType::Vertical, prev_row, site.column_});
        }

    }
    else{
        // without periodicity
        if (site.row_ == min_index) { // top edge including corners
            if(site.column_ == min_index){
                // upper left corner

                neighbors.resize(2);
                neighbors[0] = {site.row_, next_column};
                neighbors[1] = {next_row, site.column_};

                bonds.reserve(2);
                if(!_lattice.getSite(neighbors[0]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Vertical, site.row_, site.column_});
                }

            }
            else if(site.column_ == max_index){
                // upper right corner

                neighbors.resize(2);
                neighbors[0] = {site.row_, prev_column};
                neighbors[1] = {next_row, site.column_};

                bonds.reserve(2);
                if(!_lattice.getSite(neighbors[0]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Vertical, site.row_, site.column_});
                }

            }
            else{
                // top edge excluding corners
                neighbors.resize(3);
                neighbors[0] = {site.row_, next_column};
                neighbors[1] = {site.row_, prev_column};
                neighbors[2] = {next_row, site.column_};

                bonds.reserve(4);
                if(!_lattice.getSite(neighbors[0]).isActive()) {
                    bonds.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(neighbors[2]).isActive()){
                    bonds.push_back({BondType::Vertical,    site.row_, site.column_});
                }

            }
        }
        else if (site.row_ == max_index) { // bottom edge including corners
            if (site.column_ == min_index) {
                // lower left corner
                neighbors.resize(2);
                neighbors[0] = {site.row_, next_column};
                neighbors[1] = {prev_row, site.column_};

                bonds.reserve(2);
                if(!_lattice.getSite(neighbors[0]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Vertical, prev_row, site.column_});
                }

            } else if (site.column_ == max_index) {
                // lower right corner
                neighbors.resize(2);
                neighbors[0] = {site.row_, prev_column};
                neighbors[1] = {prev_row, site.column_};

                bonds.reserve(2);
                if(!_lattice.getSite(neighbors[0]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Vertical, prev_row, site.column_});
                }

            } else {
                // bottom edge excluding corners
                //  bottom edge
                neighbors.resize(3);
                neighbors[0] = {site.row_, next_column};
                neighbors[1] = {site.row_, prev_column};
                neighbors[2] = {prev_row, site.column_};

                bonds.reserve(3);
                if(!_lattice.getSite(neighbors[0]).isActive()) {
                    bonds.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(neighbors[1]).isActive()){
                    bonds.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(neighbors[2]).isActive()){
                    bonds.push_back({BondType::Vertical, prev_row, site.column_});
                }
            }
        }
            /* site.x_ > min_index && site.x_ < max_index &&  is not possible anymore*/
        else if (site.column_ == min_index) { // left edge not in the corners
            neighbors.resize(3);
            neighbors[0] = {site.row_, next_column};
            neighbors[1] = {next_row, site.column_};
            neighbors[2] = {prev_row, site.column_};

            bonds.reserve(3);
            if(!_lattice.getSite(neighbors[0]).isActive()) {
                bonds.push_back({BondType::Horizontal, site.row_, site.column_});
            }
            if(!_lattice.getSite(neighbors[1]).isActive()){
                bonds.push_back({BondType::Vertical,    site.row_, site.column_});
            }
            if(!_lattice.getSite(neighbors[2]).isActive()){
                bonds.push_back({BondType::Vertical, prev_row, site.column_});
            }
        }
        else if (site.column_ == max_index) {
            // right edge no corners

            neighbors.resize(3);
            neighbors[0] = {site.row_, prev_column};
            neighbors[1] = {next_row, site.column_};
            neighbors[2] = {prev_row, site.column_};

            bonds.reserve(3);
            if(!_lattice.getSite(neighbors[0]).isActive()){
                bonds.push_back({BondType::Horizontal, site.row_, prev_column});
            }
            if(!_lattice.getSite(neighbors[1]).isActive()){
                bonds.push_back({BondType::Vertical,    site.row_, site.column_});
            }
            if(!_lattice.getSite(neighbors[2]).isActive()){
                bonds.push_back({BondType::Vertical, prev_row, site.column_});
            }
        }
        else {
            // 1 level inside the lattice
            // not in any the boundary
            neighbors.resize(4);
            neighbors[0] = {site.row_, next_column};
            neighbors[1] = {site.row_, prev_column};
            neighbors[2] = {next_row, site.column_};
            neighbors[3] = {prev_row, site.column_};

            bonds.reserve(4);
            if(!_lattice.getSite(neighbors[0]).isActive()) {
                bonds.push_back({BondType::Horizontal, site.row_, site.column_});
            }
            if(!_lattice.getSite(neighbors[1]).isActive()){
                bonds.push_back({BondType::Horizontal, site.row_, prev_column});
            }
            if(!_lattice.getSite(neighbors[2]).isActive()){
                bonds.push_back({BondType::Vertical,    site.row_, site.column_});
            }
            if(!_lattice.getSite(neighbors[3]).isActive()){
                bonds.push_back({BondType::Vertical, prev_row, site.column_});
            }
        }

    }

    time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);

//    cout << "time_6_connection2 : " << time_6_connection2 << " sec" << endl;
}



/**
 * Take a bond index only if the corresponding site is active
 * takes longer? time than version 1?, i.e.,  connection()
 * @param site
 * @param site_neighbor
 * @param bond_neighbor
 */
void SitePercolation_ps_v8::connection_v2(Index site, vector<Index> &site_neighbor, vector<BondIndex> &bond_neighbor)
{
    clock_t t = clock();

    value_type prev_column  = (site.column_ + length() - 1) % length();
    value_type prev_row     = (site.row_ + length() - 1) % length();
    value_type next_row     = (site.row_ + 1) % length();
    value_type next_column  = (site.column_ + 1) % length();

    if(!_periodicity){
        // without periodicity
        if (site.row_ == min_index) { // top edge including corners
            if(site.column_ == min_index){
                // upper left corner

                site_neighbor.resize(2);
                site_neighbor[0] = {site.row_, next_column};
                site_neighbor[1] = {next_row, site.column_};

                bond_neighbor.reserve(2);
                if(!_lattice.getSite(site_neighbor[0]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical, site.row_, site.column_});
                }

                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;

            }
            else if(site.column_ == max_index){
                // upper right corner

                site_neighbor.resize(2);
                site_neighbor[0] = {site.row_, prev_column};
                site_neighbor[1] = {next_row, site.column_};

                bond_neighbor.reserve(2);
                if(!_lattice.getSite(site_neighbor[0]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical, site.row_, site.column_});
                }

                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;
            }
            else{
                // top edge excluding corners
                site_neighbor.resize(3);
                site_neighbor[0] = {site.row_, next_column};
                site_neighbor[1] = {site.row_, prev_column};
                site_neighbor[2] = {next_row, site.column_};

                bond_neighbor.reserve(4);
                if(!_lattice.getSite(site_neighbor[0]).isActive()) {
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(site_neighbor[2]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical,    site.row_, site.column_});
                }
                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;

            }
        }
        else if (site.row_ == max_index) { // bottom edge including corners
            if (site.column_ == min_index) {
                // lower left corner
                site_neighbor.resize(2);
                site_neighbor[0] = {site.row_, next_column};
                site_neighbor[1] = {prev_row, site.column_};

                bond_neighbor.reserve(2);
                if(!_lattice.getSite(site_neighbor[0]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
                }

                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;

            } else if (site.column_ == max_index) {
                // lower right corner
                site_neighbor.resize(2);
                site_neighbor[0] = {site.row_, prev_column};
                site_neighbor[1] = {prev_row, site.column_};

                bond_neighbor.reserve(2);
                if(!_lattice.getSite(site_neighbor[0]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
                }

                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;

            } else {
                // bottom edge excluding corners
                //  bottom edge
                site_neighbor.resize(3);
                site_neighbor[0] = {site.row_, next_column};
                site_neighbor[1] = {site.row_, prev_column};
                site_neighbor[2] = {prev_row, site.column_};

                bond_neighbor.reserve(3);
                if(!_lattice.getSite(site_neighbor[0]).isActive()) {
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
                }
                if(!_lattice.getSite(site_neighbor[1]).isActive()){
                    bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
                }
                if(!_lattice.getSite(site_neighbor[2]).isActive()){
                    bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
                }
                time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
                return;
            }
        }
            /* site.x_ > min_index && site.x_ < max_index &&  is not possible anymore*/
        else if (site.column_ == min_index) { // left edge not in the corners
            site_neighbor.resize(3);
            site_neighbor[0] = {site.row_, next_column};
            site_neighbor[1] = {next_row, site.column_};
            site_neighbor[2] = {prev_row, site.column_};

            bond_neighbor.reserve(3);
            if(!_lattice.getSite(site_neighbor[0]).isActive()) {
                bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
            }
            if(!_lattice.getSite(site_neighbor[1]).isActive()){
                bond_neighbor.push_back({BondType::Vertical,    site.row_, site.column_});
            }
            if(!_lattice.getSite(site_neighbor[2]).isActive()){
                bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
            }
            time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
            return;
        }
        else if (site.column_ == max_index) {
            // right edge no corners

            site_neighbor.resize(3);
            site_neighbor[0] = {site.row_, prev_column};
            site_neighbor[1] = {next_row, site.column_};
            site_neighbor[2] = {prev_row, site.column_};

            bond_neighbor.reserve(3);
            if(!_lattice.getSite(site_neighbor[0]).isActive()){
                bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
            }
            if(!_lattice.getSite(site_neighbor[1]).isActive()){
                bond_neighbor.push_back({BondType::Vertical,    site.row_, site.column_});
            }
            if(!_lattice.getSite(site_neighbor[2]).isActive()){
                bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
            }
            time_7_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);
            return;
        }

    }
    // 1 level inside the lattice
    // not in any the boundary
    site_neighbor.resize(4);
    site_neighbor[0] = {site.row_, next_column};
    site_neighbor[1] = {site.row_, prev_column};
    site_neighbor[2] = {next_row, site.column_};
    site_neighbor[3] = {prev_row, site.column_};

    bond_neighbor.reserve(4);
    if(!_lattice.getSite(site_neighbor[0]).isActive()) {
        bond_neighbor.push_back({BondType::Horizontal, site.row_, site.column_});
    }
    if(!_lattice.getSite(site_neighbor[1]).isActive()){
        bond_neighbor.push_back({BondType::Horizontal, site.row_, prev_column});
    }
    if(!_lattice.getSite(site_neighbor[2]).isActive()){
        bond_neighbor.push_back({BondType::Vertical,    site.row_, site.column_});
    }
    if(!_lattice.getSite(site_neighbor[3]).isActive()) {
        bond_neighbor.push_back({BondType::Vertical, prev_row, site.column_});
    }

//    time_6_connection2 += (clock() - t) / double(CLOCKS_PER_SEC);

//    cout << "time_6_connection2 : " << time_6_connection2 << " sec" << endl;
}



/**
 * This function must be called after a site is placed in the
 * newest cluster
 * and an id is given from the lattice according to the cluster they are on.
 * @param site
 */
void SitePercolation_ps_v8::save_index_if_in_boundary_v2(const Index& site){
    cout << "site with new id found " << site << " : line " << __LINE__ << endl;
    // checking row indices for Top-Bottom boundary
    if(site.row_ == min_index){ // top index
        if(!check_if_id_matches(site, _top_edge)) {
            _top_edge.push_back(site);
        }
    }
    else if(site.row_ == max_index){
        if(!check_if_id_matches(site, _bottom_edge)) {
            _bottom_edge.push_back(site);
        }
    }

    // checking column indices for Left-Right boundary
    if(site.column_ == min_index){
        if(!check_if_id_matches(site, _left_edge)) {
            _left_edge.push_back(site);
        }
    }
    else if(site.column_ == max_index){
        if(!check_if_id_matches(site, _right_edge)) {
            _right_edge.push_back(site);
        }
    }
}

/**
 *
 * @param site
 * @param edge
 * @return
 */
bool SitePercolation_ps_v8::check_if_id_matches(Index site, const vector<Index> &edge){
    for(auto s :edge){
        if(_lattice.getSite(site).get_groupID() == _lattice.getSite(s).get_groupID()){
            // no need to put the site here
//            cout << "Site " << site << " and Id " << _lattice.getSite(site).set_groupID()
//                 << " is already in the edge : line " << __LINE__ << endl;
            return true;
        }
    }
    return false;
}


/**
 * if at least one element is erased that means we have found a match and we have erased it
 * @param site
 * @param edge
 * @return
 */
bool SitePercolation_ps_v8::check_if_id_matches_and_erase(Index site, vector<Index> &edge){
    vector<Index>::iterator it = edge.begin();
    value_type sz = edge.size();
    for(; it < edge.end(); ++it){
        if(_lattice.getSite(site).get_groupID() == _lattice.getSite(*it).get_groupID()){
            // no need to put the site here
            cout << "Site " << site << " and Id " << _lattice.getSite(site).get_groupID()
                 << " is already in the edge : line " << __LINE__ << endl;
            edge.erase(it);
        }
    }
    // if at least one element is erased that means we have found a match and we have erased it
    return edge.size() < sz;
}


/***********************************************
 *  Placing sites
 *
 *****************************************/

/**
 * All site placing method in one place
 *
 * @return true if operation is successfull
 */
bool SitePercolation_ps_v8::occupy() {
    if(_index_sequence_position >= maxSites()){
        return false;
    }

//    placeSite_v6(); // ok
//    placeSite_v7(); // ok
//    placeSite_weighted_v8(); // on test
//    placeSiteWeightedRelabeling_v9(); // on test
//    placeSite_v10(); // added 2018.05.01 or so

    Index site = selectSite(); // added 2018.06.18
//    placeSite_v11(site); // added 2018.06.18
    placeSite_v13(site); // added 2018.07.16


//    cout << "_number_of_occupied_sites " << _number_of_occupied_sites << endl;
//    cout << "Cluster id set " << _cluster_id_set << endl;
//    viewCluster_id_index();
    _occuption_probability = occupationProbability(); // for super class
    return true;
}

/**
 *
 * @return false -> if not site placing is possible
 */
bool SitePercolation_ps_v8::placeSiteForSpanning() {
    if(_index_sequence_position >= maxSites()){
        return false;
    }

//    placeSite_v6();
    placeSite_v7();

//    detectSpanning();
//    detectSpanning_v2();
//    detectSpanning_v3();
//    detectSpanning_v4(_last_placed_site); // todo problem
//    detectSpanning_v5(_last_placed_site); // todo problem
    detectSpanning_v6(_last_placed_site);

    return true;
}

/**
 *
 * @return
 */
bool SitePercolation_ps_v8::placeSite_explosive(int rule){
    if(_index_sequence_position >= maxSites()){
        return false;
    }
    if(rule == 0){
        placeSite_explosive_sum_rule();
    }
    if(rule == 1){
        placeSite_explosive_prodduct_rule();
    }


    return true;
}

/**
 *
 * @return false -> if not site placing is possible
 */
bool SitePercolation_ps_v8::placeSite_explosive_product_rule_ForSpanning() {
    if(_index_sequence_position >= maxSites()){
        return false;
    }

    placeSite_explosive_prodduct_rule();
//    placeSite_explosive_test(); // comment it out

    detectSpanning_v6(_last_placed_site);

    return true;
}

/**
 *
 * @return false -> if not site placing is possible
 */
bool SitePercolation_ps_v8::placeSite_explosive_sum_rule_ForSpanning() {
    if(_index_sequence_position >= maxSites()){
        return false;
    }

    placeSite_explosive_sum_rule();
//    placeSite_explosive_test(); // comment it out

    detectSpanning_v6(_last_placed_site);

    return true;
}


/**
 * for Canonical Ensemble
 * used to calculate quantities using convolution process.
 * select a site sequentially
 * Generate a random number between [0,1]
 *      if it is greater than p, then place the site
 *      else select next site and go the previous step
 * @param p  -> probability. possible values [0,1]
 */
void SitePercolation_ps_v8::placeSite_sequentially(double p) {
    double r;
    for(value_type i{} ; i != index_sequence.size(); ++i){
        r = std::rand() / double(RAND_MAX);
        if(r <= p){
            // place the site index_sequence[i]
            Index current_site = index_sequence[i];
            _last_placed_site = current_site;

            _lattice.activate_site(current_site);

            ++_number_of_occupied_sites;

            // find the bonds for this site
            vector<BondIndex> bonds;
            vector<Index>     neighbors;
    //    connection_v1(current_site, neighbors, bonds);
            connection_v2(current_site, neighbors, bonds);

            // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
            set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(neighbors);

            value_type merged_cluster_index = manage_clusters_v10(
                    found_index_set, bonds, current_site
            );

        }
    }
}




/***
 * Index of the selected site must be provided with the argument
 *
 * Wrapping and spanning index arrangement is enabled.
 * Entropy is calculated smoothly.
 * Entropy is measured by site and bond both.
 * @param current_site
 * @return
 */
value_type SitePercolation_ps_v8::placeSite_v13(Index current_site) {
    // randomly choose a site
    if (_number_of_occupied_sites == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    _last_placed_site = current_site;
//    cout << "placing site " << current_site << endl;

    _lattice.activate_site(current_site);

    ++_number_of_occupied_sites;


    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     sites;

//    connection_v1(current_site, sites, bonds);
    connection_v2(current_site, sites, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(sites);

//    cout << "Found indices " << found_index_set << endl;


    subtract_entropy_for_full(found_index_set);  // tracking entropy change

    auto t0 = chrono::system_clock::now();
    value_type merged_cluster_index = manage_clusters_v10(
            found_index_set, bonds, current_site
    );
    auto t1 = chrono::system_clock::now();
    time_relabel += chrono::duration<double>(t1 - t0).count();


    add_entropy_for_full(merged_cluster_index); // tracking entropy change

    // running tracker
    track_numberOfBondsInLargestCluster(); // tracking number of bonds in the largest cluster

    return merged_cluster_index;
}

/***
 * Index of the selected site must be provided with the argument
 *
 * Wrapping and spanning index arrangement is enabled.
 * Entropy is calculated smoothly.
 * Entropy is measured by site and bond both.
 * @param current_site
 * @return
 */
value_type SitePercolation_ps_v8::placeSite_v14(Index current_site) {
    // randomly choose a site
    if (_number_of_occupied_sites == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    _last_placed_site = current_site;
//    cout << "placing site " << current_site << endl;

    _lattice.activate_site(current_site);

    ++_number_of_occupied_sites;


    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     sites;

//    connection_v1(current_site, sites, bonds);
    connection_v2(current_site, sites, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set;
    value_type  base = find_index_for_placing_new_bonds_v4(sites, found_index_set);

//    cout << "Found indices " << found_index_set << endl;


    subtract_entropy_for_full(found_index_set);  // tracking entropy change

    auto t0 = chrono::system_clock::now();
    value_type merged_cluster_index = manage_clusters_v13(
            found_index_set, bonds, current_site, base
    );
    auto t1 = chrono::system_clock::now();
    time_relabel += chrono::duration<double>(t1 - t0).count();


    add_entropy_for_full(merged_cluster_index); // tracking entropy change

    // running tracker
    track_numberOfBondsInLargestCluster(); // tracking number of bonds in the largest cluster

    return merged_cluster_index;
}



/**
 *
 * @return
 */
Index SitePercolation_ps_v8::selectSite(){
    Index current_site = randomized_index_sequence[_index_sequence_position];
    ++_index_sequence_position;

    return current_site;
}

/**
 * 
 * @param site 
 * @return 
 */
value_type SitePercolation_ps_v8::cluster_length_for_placing_site_product_rule(Index site){
    vector<Index> neighbor_site;
    vector<BondIndex> neighbor_bond;
    connection_v2(site, neighbor_site, neighbor_bond);
    value_type prod = 1;
    int id;
    size_t index;
//    cout << "for " << site << " there are " << neighbor_site.size() << " neighbors ";
    for(auto s: neighbor_site){
        id = _lattice.getSite(s).get_groupID();
        if(id < 0){
            prod *= 1;  // cluster of 1 bond
            continue;
        }
        index = _cluster_index_from_id[id];
        prod *= _clusters[index].numberOfBonds();
    }
//    cout << "product = " << prod << endl;
    return prod;
}

/**
 * 
 * @param site 
 * @return 
 */
value_type SitePercolation_ps_v8::cluster_length_for_placing_site_sum_rule(Index site){
    vector<Index> neighbor_site;
    vector<BondIndex> neighbor_bond;
    connection_v2(site, neighbor_site, neighbor_bond);
    value_type sum = 0;
    int id;
    size_t index;
//    cout << "for " << site << " there are " << neighbor_site.size() << " neighbors ";
    for(auto s: neighbor_site){
        id = _lattice.getSite(s).get_groupID();
        if(id < 0){
            sum += 1;   // cluster of 1 bond
            continue;
        }
        index = _cluster_index_from_id[id];
        sum += _clusters[index].numberOfBonds();
    }
//    cout << "sum = " << sum << endl;
    return sum;
}



/**
 * Definition :
 *      number of bonds of each neighbors are multiplied
 *      if more than one neighbor is in the same cluster, take cluster size = 1 for all neighbors but one
 * @param site
 * @return
 */
value_type SitePercolation_ps_v8::cluster_length_for_placing_site_product_rule_v2(Index site){
    vector<Index> neighbor_site;
    vector<BondIndex> neighbor_bond;
    connection_v2(site, neighbor_site, neighbor_bond);
    value_type prod = 1;
    int id;
    size_t index;
//    cout << "for " << site << " there are " << neighbor_site.size() << " neighbors ";
    vector<int> ids;
    bool present{false};
    for(auto s: neighbor_site){
        id = _lattice.getSite(s).get_groupID();
        if(id < 0){
            prod *= 1;  // cluster of 1 bond
            continue;
        }
        for(auto i: ids){
            if(id == i)
                present = true;
        }
        if(present){
            prod *= 1;
            present = false;
            continue;
        }
        ids.push_back(id);
        index = _cluster_index_from_id[id];
        prod *= _clusters[index].numberOfBonds();
    }
//    cout << "product = " << prod << endl;
    return prod;
}

/**
 * Definition :
 *      number of bonds of each neighbors are summed over
 *      if more than one neighbor is in the same cluster, take cluster size = 1 for all neighbors but one
 * @param site
 * @return
 */
value_type SitePercolation_ps_v8::cluster_length_for_placing_site_sum_rule_v2(Index site){
    vector<Index> neighbor_site;
    vector<BondIndex> neighbor_bond;
    connection_v2(site, neighbor_site, neighbor_bond);
    value_type sum = 0;
    int id;
    size_t index;
//    cout << "for " << site << " there are " << neighbor_site.size() << " neighbors ";
    vector<int> ids;
    bool present{false};
    for(auto s: neighbor_site){
        id = _lattice.getSite(s).get_groupID();
        if(id < 0){
            sum += 1;   // cluster of 1 bond
            continue;
        }
        for(auto i: ids){
            if(id == i)
                present = true;
        }
        if(present){
            sum += 1;
            present = false;
            continue;
        }
        ids.push_back(id);
        index = _cluster_index_from_id[id];
        sum += _clusters[index].numberOfBonds();
    }
//    cout << "sum = " << sum << endl;
    return sum;
}



/**
 * Randomly choose two site say siteA and siteB.
 * calculate for which site cluster length is minimum (by product rule or sum rule)
 * place that site for which cluster length is minimum
 * 
 * @return merged_cluster_index -> the index of _cluster
 *                                      where new site and bond indicis are placed
 *                                      or the merged cluster index
 *
 */
value_type SitePercolation_ps_v8::placeSite_explosive_prodduct_rule() {
    // randomly choose a site
    if (_index_sequence_position == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    // in order to apply sum rule or product rule
    size_t i = _index_sequence_position;
    Index current_site = randomized_index_sequence[i];
    if (i != randomized_index_sequence.size()-1) {
        size_t j = (i + 1) + rand() % (randomized_index_sequence.size() - (i + 1));
        Index siteA = randomized_index_sequence[i];
        Index siteB = randomized_index_sequence[j];

        value_type lenA = cluster_length_for_placing_site_product_rule(siteA);
        value_type lenB = cluster_length_for_placing_site_product_rule(siteB);
        if (lenA < lenB) {
            // place siteA
            current_site = siteA;
            // cout << "selecting " << siteA << " since product is " << lenA << endl;
            // use siteA as current site
        } else {
            current_site = siteB;
            // cout << "selecting " << siteB << " since product is " << lenB << endl;
            // flip the sites since siteB was selected from free region.
            // if it is not done siteA will not be placed at all
            // since _index_sequence_position is always increases by 1
            randomized_index_sequence[i] = siteB;
            randomized_index_sequence[j] = siteA;
        }
    }
    _lattice.activate_site(current_site);
    ++_number_of_occupied_sites;
    ++_index_sequence_position;
    _last_placed_site = current_site;

    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     neighbors;
//    connection_v1(current_site, neighbors, bonds);
    connection_v2(current_site, neighbors, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(neighbors);

//    cout << "Found indices " << found_index_set << endl;

    subtract_entropy_for_bond(found_index_set);
    value_type merged_cluster_index = manage_clusters_v7(
            found_index_set, bonds, current_site
    );
    add_entropy_for_bond(merged_cluster_index);

    // running tracker
    track_numberOfBondsInLargestCluster();

    
    return merged_cluster_index;
}



/**
 * Randomly choose two site say siteA and siteB.
 * calculate for which site cluster length is minimum (by product rule or sum rule)
 * place that site for which cluster length is minimum
 *
 * @return merged_cluster_index -> the index of _cluster
 *                                      where new site and bond indicis are placed
 *                                      or the merged cluster index
 *
 */
value_type SitePercolation_ps_v8::placeSite_explosive_sum_rule() {
    // randomly choose a site
    if (_index_sequence_position == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    // in order to apply sum rule or product rule
    size_t i = _index_sequence_position;
    Index current_site = randomized_index_sequence[i];
    if (i != randomized_index_sequence.size()-1) {

        size_t j = (i + 1) + rand() % (randomized_index_sequence.size() - (i + 1));
        Index siteA = randomized_index_sequence[i];
        Index siteB = randomized_index_sequence[j];

        value_type lenA = cluster_length_for_placing_site_sum_rule(siteA);
        value_type lenB = cluster_length_for_placing_site_sum_rule(siteB);
        if (lenA < lenB) {
            // place siteA
            current_site = siteA;
            // cout << "selecting " << siteA << endl;
            // use siteA as current site
        } else {
            current_site = siteB;
            // cout << "selecting " << siteB << endl;
            // flip the sites since siteB was selected from free region.
            // if it is not done siteA will not be placed at all
            // since _index_sequence_position is always increases by 1
            randomized_index_sequence[i] = siteB;
            randomized_index_sequence[j] = siteA;
        }
    }

    _lattice.activate_site(current_site);
    ++_number_of_occupied_sites;
    ++_index_sequence_position;
    _last_placed_site = current_site;

    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     neighbors;
//    connection_v1(current_site, neighbors, bonds);
    connection_v2(current_site, neighbors, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(neighbors);

//    cout << "Found indices " << found_index_set << endl;

    subtract_entropy_for_bond(found_index_set);
    value_type merged_cluster_index = manage_clusters_v7(
            found_index_set, bonds, current_site
    );
    add_entropy_for_bond(merged_cluster_index);

    // running tracker
    track_numberOfBondsInLargestCluster();


    return merged_cluster_index;
}


/**
 * Randomly choose two site say siteA and siteB.
 * calculate for which site cluster length is minimum (by product rule or sum rule)
 * place that site for which cluster length is minimum
 *
 * @return merged_cluster_index -> the index of _cluster
 *                                      where new site and bond indicis are placed
 *                                      or the merged cluster index
 *
 */
value_type SitePercolation_ps_v8::placeSite_explosive_test() {
    // randomly choose a site
    if (_index_sequence_position == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    // in order to apply sum rule or product rule
    size_t i = _index_sequence_position;
    size_t j = (i+1) + rand() % (randomized_index_sequence.size() - (i+1));
    Index siteA = randomized_index_sequence[i];
    Index siteB = randomized_index_sequence[j];

//    cout << "placing site " << current_site << endl;

    Index current_site;
    value_type lenA_prod = cluster_length_for_placing_site_product_rule(siteA);
    value_type lenB_prod = cluster_length_for_placing_site_product_rule(siteB);
    value_type lenA_sum = cluster_length_for_placing_site_sum_rule(siteA);
    value_type lenB_sum = cluster_length_for_placing_site_sum_rule(siteB);
    if(lenA_prod < lenB_prod && lenA_sum > lenB_sum){
        cerr << "lenA_prod < lenB_prod && lenA_sum > lenB_sum" << endl;
    }
    if(lenA_prod > lenB_prod && lenA_sum < lenB_sum){
        cerr << "lenA_prod > lenB_prod && lenA_sum < lenB_sum" << endl;
    }
    if(lenA_prod < lenB_prod){
        // place siteA
        current_site = siteA;
         cout << "selecting " << siteA << endl;
        // use siteA as current site
    }else{
        current_site = siteB;
        cout << "selecting " << siteB  << endl;
        // flip the sites since siteB was selected from free region.
        // if it is not done siteA will not be placed at all
        // since _index_sequence_position is always increases by 1
        randomized_index_sequence[i] = siteB;
        randomized_index_sequence[j] = siteA;
    }

    _lattice.activate_site(current_site);
    ++_number_of_occupied_sites;
    ++_index_sequence_position;
    _last_placed_site = current_site;

    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     neighbors;
//    connection_v1(current_site, neighbors, bonds);
    connection_v2(current_site, neighbors, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(neighbors);

//    cout << "Found indices " << found_index_set << endl;

    subtract_entropy_for_bond(found_index_set);
    value_type merged_cluster_index = manage_clusters_v7(
            found_index_set, bonds, current_site
    );
    add_entropy_for_bond(merged_cluster_index);

    // running tracker
    track_numberOfBondsInLargestCluster();


    return merged_cluster_index;
}


/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::placeSite_with_weight(){
    cout << "Not implemented : line " << __LINE__ << endl;
    return 0;
}



/***************************************************
 * View methods
 ****************************************/

void SitePercolation_ps_v8::viewCluster_id_index(){
    cout << "set_ID->index : line " << __LINE__ << endl;
    cout << _cluster_index_from_id << endl;
//    for(size_t c{}; c != _clusters.size() ; ++c){
//        auto cifid = _cluster_index_from_id[_clusters[c].set_ID()];
//        cout << "cluster.set_ID() " << _clusters[c].get_ID() << ", Index " << cifid << endl;
//    }
}


// recreate cluster from active sites
// for debugging purposes
void SitePercolation_ps_v8::view_cluster_from_ground_up() {
    cout << "Re implementation required : line " << __LINE__ << endl;
    return;

}


/**
 *
 */
void SitePercolation_ps_v8::spanningIndices() const {
    cout << "Spanning Index : id" << endl;
    for(auto i: _spanning_sites){
        cout << "Index " << i << " : id " << _lattice.getSite(i).get_groupID() << endl;
    }
}

void SitePercolation_ps_v8::wrappingIndices() const {
    cout << "Wrapping Index : id : relative index" << endl;
    for(auto i: _wrapping_sites){
        cout << "Index " << i << " : id "
             << _lattice.getSite(i).get_groupID()
             << " relative index : " << _lattice.getSite(i).relativeIndex() << endl;
    }
}


/****************************************
 * Spanning Detection
 ****************************************/


/**
 * todo problem
 * successful : but result is not entirely correct
 * length       time
 * 200          5.265000 sec
 * 500          2 min 29.656000
 * @param site : Check spanning for this argument
 * @return
 */
bool SitePercolation_ps_v8::detectSpanning_v5(const Index& site) {
    if (debug_5_detectSpanning) {
        cout << "Entry -> detectSpanning_v4() : line " << __LINE__ << endl;
    }
    if(_periodicity) {
        cout << "Cannot detect spanning if _periodicity if ON: line " << __LINE__ << endl;
        return false;
    }


    // first check if the site with a cluster id is already a spanning site
    for(const Index& ss: _spanning_sites){
        if(_lattice.getSite(ss).get_groupID() == _lattice.getSite(site).get_groupID()){
            cout << "Already a spanning site : line " << __LINE__ << endl;
            return true;
        }
    }

    // only check for the newest site placed
    if(site.row_ == min_index){ // top index
        _top_edge.push_back(site);
    }
    else if(site.row_ == max_index){
        _bottom_edge.push_back(site);
    }

    // checking column indices for Left-Right boundary
    if(site.column_ == min_index){ // left edge
        _left_edge.push_back(site);
    }
    else if(site.column_ == max_index){
        _right_edge.push_back(site);
    }

    if(_index_sequence_position < length()){
//        cout << "Not enough site to span : line " << __LINE__ << endl;
        return false;
    }

    // now do the matching with top and bottom for vertical spanning
    // meaning new site is added to _spanning_sites so remove them from top and bottom edges
    vector<Index>::iterator it_top = _top_edge.begin();
    vector<Index>::iterator it_bot = _bottom_edge.begin();
    bool found_spanning_site = false;
    for(; it_top < _top_edge.end(); ++it_top){
        for(; it_bot < _bottom_edge.end(); ++it_bot){
            if(_lattice.getSite(*it_top).get_groupID() == _lattice.getSite(*it_bot).get_groupID()){
                _spanning_sites.push_back(*it_top);
//                _spanning_occured = true;
//                spanning_id = _lattice.getSite(*it_bot).set_groupID();
                found_spanning_site = true;
                _bottom_edge.erase(it_bot);
            }
        }
        if(found_spanning_site){
            found_spanning_site = false;
            _top_edge.erase(it_top);
        }
    }


    found_spanning_site = false;
    // now do the matching with left and right for horizontal spanning
    // meaning new site is added to _spanning_sites so remove them from top and bottom edges
    vector<Index>::iterator it_lft = _left_edge.begin();
    vector<Index>::iterator it_rht = _right_edge.begin();
    for(; it_lft < _left_edge.end(); ++it_lft){
        for(; it_rht < _right_edge.end(); ++it_rht){
            if(_lattice.getSite(*it_lft).get_groupID() == _lattice.getSite(*it_rht).get_groupID()){
                _spanning_sites.push_back(*it_lft);
//                _spanning_occured = true;
//                spanning_id = _lattice.getSite(*it_bot).set_groupID();
                found_spanning_site = true;
                _right_edge.erase(it_rht);
            }
        }
        if(found_spanning_site){
            found_spanning_site = false;
            _left_edge.erase(it_lft);
        }
    }


    return !_spanning_sites.empty();
}


/**
 * success : gives correct result
 * length       time
 * 200          7.859000 sec
 * 500          2 min 18.874000 sec
 * only check for the cluster id of the recently placed site
 * @param site : Check spanning for this argument
 * @return
 */
bool SitePercolation_ps_v8::detectSpanning_v6(const Index& site) {
    if (debug_5_detectSpanning) {
        cout << "Entry -> detectSpanning_v4() : line " << __LINE__ << endl;
    }
    if(_periodicity) {
        cout << "Cannot detect spanning if _periodicity if ON: line " << __LINE__ << endl;
        return false;
    }

    // first check if the site with a cluster id is already a spanning site
    for(const Index& ss: _spanning_sites){
        if(_lattice.getSite(ss).get_groupID() == _lattice.getSite(site).get_groupID()){
//            cout << "Already a spanning site : line " << __LINE__ << endl;
            return true;
        }
    }

    // only check for the newest site placed
    if(site.row_ == min_index){ // top index
        if(!check_if_id_matches(site, _top_edge)) {
            _top_edge.push_back(site);
        }
    }
    else if(site.row_ == max_index){
        if(!check_if_id_matches(site, _bottom_edge)){
            _bottom_edge.push_back(site);
        }
    }

    // checking column indices for Left-Right boundary
    if(site.column_ == min_index){ // left edge
        if(!check_if_id_matches(site, _left_edge)) {
            _left_edge.push_back(site);
        }
    }
    else if(site.column_ == max_index){
        if(!check_if_id_matches(site, _right_edge)) {
            _right_edge.push_back(site);
        }
    }

    if(_number_of_occupied_sites < length()){
//        cout << "Not enough site to span : line " << __LINE__ << endl;
        return false;
    }


    vector<Index>::iterator it_top = _top_edge.begin();
    vector<Index>::iterator it_bot = _bottom_edge.begin();
    bool found_spanning_site = false;
    int id = _lattice.getSite(site).get_groupID();

    if(_top_edge.size() < _bottom_edge.size()){
        // if matched found on the smaller edge look for match in the larger edge
        for(; it_top < _top_edge.end(); ++it_top){
            if(id == _lattice.getSite(*it_top).get_groupID()){
                for(; it_bot < _bottom_edge.end(); ++it_bot){
                    if(id == _lattice.getSite(*it_bot).get_groupID()){
                        // match found !
                        if(!check_if_id_matches(*it_top ,_spanning_sites)) {
//                            _spanning_occured = true;
                            _spanning_sites.push_back(*it_top);
                        }
                        found_spanning_site = true;
                        _bottom_edge.erase(it_bot);
                    }
                }

                if(found_spanning_site){
                    found_spanning_site = false;
                    _top_edge.erase(it_top);
                }

            }
        }
    }else{
        for (; it_bot < _bottom_edge.end(); ++it_bot) {
            if (id == _lattice.getSite(*it_bot).get_groupID()) {
                for (; it_top < _top_edge.end(); ++it_top) {
                    if (id == _lattice.getSite(*it_top).get_groupID()) {
                        // match found !
                        if (!check_if_id_matches(*it_top, _spanning_sites)) {
//                            _spanning_occured = true;
                            _spanning_sites.push_back(*it_top);
                        }
                        found_spanning_site = true;
                        _top_edge.erase(it_top);
                    }
                }
                if(found_spanning_site){
                    found_spanning_site = false;
                    _bottom_edge.erase(it_top);
                }
            }
        }

    }

    found_spanning_site = false;
    vector<Index>::iterator it_lft = _left_edge.begin();
    vector<Index>::iterator it_rht = _right_edge.begin();

    if(_left_edge.size() < _right_edge.size()){
        for(; it_lft < _left_edge.end(); ++it_lft) {
            if (id == _lattice.getSite(*it_lft).get_groupID()) {
                for (; it_rht < _right_edge.end(); ++it_rht) {
                    if (id == _lattice.getSite(*it_rht).get_groupID()) {
                        if (!check_if_id_matches(*it_lft, _spanning_sites)) {
                            _spanning_sites.push_back(*it_lft);
//                            _spanning_occured = true;
                        }
                        found_spanning_site = true;
                        _right_edge.erase(it_rht);
                    }
                }
                if (found_spanning_site) {
                    found_spanning_site = false;
                    _left_edge.erase(it_lft);
                }
            }
        }
    }else{
        for (; it_rht < _right_edge.end(); ++it_rht) {
            if (id == _lattice.getSite(*it_rht).get_groupID()) {
                for(; it_lft < _left_edge.end(); ++it_lft) {
                    if (id == _lattice.getSite(*it_lft).get_groupID()) {
                        if (!check_if_id_matches(*it_lft, _spanning_sites)) {
                            _spanning_sites.push_back(*it_lft);
//                            _spanning_occured = true;
                        }
                        found_spanning_site = true;
                        _left_edge.erase(it_lft);
                    }
                }
                if (found_spanning_site) {
                    found_spanning_site = false;
                    _right_edge.erase(it_rht);
                }
            }
        }
    }


    // now do the matching with left and right for horizontal spanning
    // meaning new site is added to _spanning_sites so remove them from top and bottom edges



    // filter spanning ids


    return !_spanning_sites.empty();

}


/**
 * todo scan the edges and save it in the edge indices
 */
void SitePercolation_ps_v8::scanEdges() {

}




/***********************************
 * Wrapping Detection
 **********************************/
/**
 * Wrapping is detected here using the last placed site
 * @return bool. True if wrapping occured.
 */
bool SitePercolation_ps_v8::detectWrapping() {
    Index site = lastPlacedSite();
    // only possible if the cluster containing 'site' has sites >= length of the lattice
    if(_number_of_occupied_sites < length()){
        return false;
    }

    // check if it is already a wrapping site
    int id = _lattice.getSite(site).get_groupID();
    int tmp_id{};
    for (auto i: _wrapping_sites){
        tmp_id = _lattice.getSite(i).get_groupID();
        if(id == tmp_id ){
//            cout << "Already a wrappig cluster : line " << __LINE__ << endl;
            return true;
        }
    }

    // get four neighbors of site always. since wrapping is valid if periodicity is implied
    vector<Index> sites = _lattice.get_neighbor_site_indices(site);


    if(sites.size() < 2){ // at least two neighbor of  site is required
        return false;
    }else{
        IndexRelative irel = _lattice.getSite(site).relativeIndex();
//        cout << "pivot's " << site << " relative " << irel << endl;
        IndexRelative b;
        for (auto a:sites){
            if(_lattice.getSite(a).get_groupID() != _lattice.getSite(site).get_groupID()){
                // different cluster
                continue;
            }
//            cout << "belongs to the same cluster : line " << __LINE__ << endl;

            b = _lattice.getSite(a).relativeIndex();
//            cout << "neibhbor " << a << " relative " << b << endl;
            if(abs(irel.x_ - b.x_) > 1 || abs(irel.y_ - b.y_) > 1){
//                cout << "Wrapping : line " << __LINE__ << endl;
                _wrapping_sites.push_back(site);
                return true;
            }
        }
    }

//    cout << "wrapping sites " << _wrapping_indices << endl;
    // if %_wrapping_indices is not empty but wrapping is not detected for the current site (%site)
    // that means there is wrapping but not for the %site
    return !_wrapping_sites.empty();
}




/**************************************************
 * Distributions
 *
 **************************************************/

std::vector<value_type> SitePercolation_ps_v8::number_of_site_in_clusters() {
    vector<value_type> x;
    x.reserve(_clusters.size());
    for(auto a: _clusters){
        x.push_back(a.numberOfSites());
    }
    return x;
}


/**
 *
 * @return
 */
std::vector<value_type> SitePercolation_ps_v8::number_of_bonds_in_clusters() {
    vector<value_type> x;
    x.reserve(_clusters.size());
    value_type nob, total{};

    for(auto a: _clusters){
        nob = a.numberOfBonds();
        x.push_back(nob);
        total += nob;
    }

    value_type mnob = maxBonds();
    x.reserve(mnob - total);
    while (total < mnob){
        x.push_back(1);
        total += 1;
    }

    return x;
}


/********************************************************
 *
 *
 ******************************************************/
/**
 *
 */
void SitePercolation_ps_v8::periodicity_status() {
    cout << "Periodicity  " << (_periodicity ? "On" : "Off") << endl;

}



/**
 *
 * @return Size of the first spanning cluster, i.e., number of bonds in the spanning cluster
 */
value_type SitePercolation_ps_v8::placeSiteUntilFirstSpanning_v2() {
    bool temp = _periodicity;
    _periodicity = false;

    while (placeSiteForSpanning()){

        if(isSpanned()){
            break;
        }
    }

    _periodicity = temp;
    // spanning cluster index

//    if(_first_spanning_cluster_id == -1){
//        cout << "Spanning detected but _first_spanning_cluster_id == -1 : line " << __LINE__ << endl;
//        return 0;
//    }
    return value_type(_lattice.getSite(_spanning_sites.front()).get_groupID());
}


/**
 *
 * @return Size of the first spanning cluster, i.e., number of bonds in the spanning cluster
 */
/**
 *
 * @param rule: 0 for sum rule, 1 for product rule
 * @return
 */
value_type SitePercolation_ps_v8::placeSite_explosive_UntilFirstSpanning(int rule) {
    bool temp = _periodicity;
    _periodicity = false;

    if(rule == 0) {
        while (placeSite_explosive_sum_rule_ForSpanning()) {

            if (isSpanned()) {
                break;
            }
        }
    }
    else if(rule == 1){
        while (placeSite_explosive_product_rule_ForSpanning()) {

            if (isSpanned()) {
                break;
            }
        }
    }
    else{
        cout << "Undefined method : line " << __LINE__ << endl;
        cout << "rule can be 0 or 1 for sum rule and product rule respectively" << endl;
        cout << "calling exit(1)" << endl;
        exit(1);
    }

    _periodicity = temp;
    // spanning cluster index

//    if(_first_spanning_cluster_id == -1){
//        cout << "Spanning detected but _first_spanning_cluster_id == -1 : line " << __LINE__ << endl;
//        return 0;
//    }
    return value_type(_lattice.getSite(_spanning_sites.front()).get_groupID());
}


/***
 *
 * @param id
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInCluster_by_id(value_type id){
    value_type x{};
    for(auto a: _clusters){
        if(a.get_ID() == id){
            x = a.numberOfBonds();
            return x;
        }
    }
    cout << "cluster wiht set_ID " << id << " does not exists : line " << __LINE__ << endl;
    return x;
}

value_type SitePercolation_ps_v8::numberOfSitesInCluster_by_id(value_type id) {
    value_type x{};
    for(auto a: _clusters){
        if(a.get_ID() == id){
            x = a.numberOfSites();
        }
    }
    return x;
}


/********************************************************************
 * Relabeling
 *
 *********************************************************************/

/**
 * Takes most of the time ~6 second when total runtime is ~8.7 sec for Length = 100 // todo
 * @param start
 */
//void SitePercolation_ps_v8::relabelMap3(){
//    // no more needed when using InverseArray class
//}


/**
 *
 * @param clstr
 * @param id
 */
void SitePercolation_ps_v8::relabel_sites(const Cluster_v2& clstr, int id) {
    const vector<Index> sites = clstr.getSiteIndices();
    for(auto a: sites){
        _lattice.getSite(a).set_groupID(id);
    }
}


/**
 * Relabels site and also reassign relative index to the relabeled sites
  *
  * @param site_a  : root index of the base cluster
  * @param clstr_b : 2nd cluster, which to be merged withe the root
  */
void SitePercolation_ps_v8::relabel_sites_v4(Index site_a, const Cluster_v2& clstr_b) {
    const vector<Index> sites = clstr_b.getSiteIndices();
    int id_a = _lattice.getSite(site_a).get_groupID();
    int id_b = clstr_b.get_ID();
    Index b = clstr_b.getRootSite();

    // get four site_b of site_a
    vector<Index> sites_neighbor_a = _lattice.get_neighbor_site_indices(site_a);
    Index site_b;
    IndexRelative relative_index_b_after;
    bool flag{false};
    // find which site_b has id_a of clstr_b
    for(auto n: sites_neighbor_a){
        if(id_b == _lattice.getSite(n).get_groupID()){
            // checking id_a equality is enough. since id_a is the id_a of the active site already.
            relative_index_b_after = getRelativeIndex(site_a, n);
            site_b = n;
//            cout << "neighbor  of" << site_a << " is " << site_b << endl;
            flag = true;
            break; // todo ?

        }
    }

    if(!flag){
        cout << "No neibhgor found! : line " << __LINE__ << endl;
    }


    IndexRelative relative_site_a = _lattice.getSite(site_a).relativeIndex();

    // with this delta_a and delta_y find the relative index of site_b while relative index of site_a is known
    IndexRelative relative_site_b_before = _lattice.getSite(site_b).relativeIndex();

    int delta_x_ab = relative_index_b_after.x_ - relative_site_b_before.x_;
    int delta_y_ab = relative_index_b_after.y_ - relative_site_b_before.y_;

//    cout << relative_index_b_after << " - " << relative_site_b_before << " = delta_x, delta_y = " << delta_x_ab << ", " << delta_y_ab << endl;
    int x, y;

    for (auto a : sites) {
        _lattice.getSite(a).set_groupID(id_a);
        relative_site_a = _lattice.getSite(a).relativeIndex();

//        cout << "Relative " << relative_site_a << endl;
        x = relative_site_a.x_ + delta_x_ab;
        y = relative_site_a.y_ + delta_y_ab;
//        cout << "(x,y) = (" << x << "," << y << ") : line " << __LINE__ << endl;
        _lattice.getSite(a).relativeIndex(x, y);
    }
}


/**
 * Relabels site and also reassign relative index to the relabeled sites
  *
  * @param site_a  : last added site index of the base cluster
  * @param clstr_b : 2nd cluster, which to be merged withe the root
  */
void SitePercolation_ps_v8::relabel_sites_v5(Index site_a, const Cluster_v2& clstr_b) {
    const vector<Index> sites = clstr_b.getSiteIndices();
    int id_a = _lattice.getSite(site_a).get_groupID();
    int id_b = clstr_b.get_ID();
    Index b = clstr_b.getRootSite();

    // get four site_b of site_a
    vector<Index> sites_neighbor_a = _lattice.get_neighbor_site_indices(site_a);
    Index site_b;
    IndexRelative relative_index_b_after;
    bool flag{false};
    // find which site_b has id_a of clstr_b
    for(auto n: sites_neighbor_a){
        if(id_b == _lattice.getSite(n).get_groupID()){
            // checking id_a equality is enough. since id_a is the id_a of the active site already.
            relative_index_b_after = getRelativeIndex(site_a, n);
            site_b = n;
//            cout << "neighbor  of" << site_a << " is " << site_b << endl;
            flag = true;
            break; // todo ?

        }
    }

    if(!flag){
        cout << "No neibhgor found! : line " << __LINE__ << endl;
    }


    IndexRelative relative_site_a = _lattice.getSite(site_a).relativeIndex();

    // with this delta_a and delta_y find the relative index of site_b while relative index of site_a is known
    IndexRelative relative_site_b_before = _lattice.getSite(site_b).relativeIndex();

    int delta_x_ab = relative_index_b_after.x_ - relative_site_b_before.x_;
    int delta_y_ab = relative_index_b_after.y_ - relative_site_b_before.y_;

//    cout << relative_index_b_after << " - " << relative_site_b_before << " = delta_x, delta_y = " << delta_x_ab << ", " << delta_y_ab << endl;

    relabel_sites(sites, id_a, delta_x_ab, delta_y_ab);

}



/**
 * Relabels site and also reassign relative index to the relabeled sites
 *
 * @param site_a  : root index of the base cluster or last added site index of the base cluster
 * @param clstr_b : 2nd cluster, which to be merged withe the root
 * @param id : id to be used for relabeling sites
 */
void SitePercolation_ps_v8::relabel_sites_v6(Index site_a, const Cluster_v2& clstr_b, int id) {
    const vector<Index> sites = clstr_b.getSiteIndices();
    int id_a = _lattice.getSite(site_a).get_groupID();
    int id_b = clstr_b.get_ID();
    Index b = clstr_b.getRootSite();

    // get four site_b of site_a
    vector<Index> sites_neighbor_a = _lattice.get_neighbor_site_indices(site_a);
    Index site_b;
    IndexRelative relative_index_b_after;
    bool flag{false};
    // find which site_b has id_a of clstr_b
    for(auto n: sites_neighbor_a){
        if(id_b == _lattice.getSite(n).get_groupID()){
            // checking id_a equality is enough. since id_a is the id_a of the active site already.
            relative_index_b_after = getRelativeIndex(site_a, n);
            site_b = n;
//            cout << "neighbor  of" << site_a << " is " << site_b << endl;
            flag = true;
            break; // todo ?

        }
    }

    if(!flag){
        cout << "No neibhgor found! : line " << __LINE__ << endl;
    }


    IndexRelative relative_site_a = _lattice.getSite(site_a).relativeIndex();

    // with this delta_a and delta_y find the relative index of site_b while relative index of site_a is known
    IndexRelative relative_site_b_before = _lattice.getSite(site_b).relativeIndex();

    int delta_x_ab = relative_index_b_after.x_ - relative_site_b_before.x_;
    int delta_y_ab = relative_index_b_after.y_ - relative_site_b_before.y_;

//    cout << relative_index_b_after << " - " << relative_site_b_before << " = delta_x, delta_y = " << delta_x_ab << ", " << delta_y_ab << endl;

    relabel_sites(sites, id, delta_x_ab, delta_y_ab);

}



void SitePercolation_ps_v8::relabel_sites(const vector<Index> &sites, int id_a, int delta_x_ab, int delta_y_ab)  {
    int x, y;
    Index a;
    IndexRelative relative_site__a;
    for (value_type i = 0; i < sites.size(); ++i) {
        a = sites[i];
        _lattice.getSite(a).set_groupID(id_a);
        relative_site__a = _lattice.getSite(a).relativeIndex();
        x = relative_site__a.x_ + delta_x_ab;
        y = relative_site__a.y_ + delta_y_ab;
        _lattice.getSite(a).relativeIndex(x, y);
    }
}

/**
 *
 * @param clstr
 * @param id
 */
void SitePercolation_ps_v8::relabel_bonds(const Cluster_v2& clstr, int id) {
    vector<BondIndex> bonds = clstr.getBondIndices();
    for(auto a: bonds){
        _lattice.getBond(a).set_groupID(id);
    }
}


/**********************************************
 * Information about current state of Class
 **********************************************/
/**
 * number of bonds in spanning cluster / total number of bonds
 * here, total number of bonds = (2*l*l - 2*l)
 *  since periodic boundary condition is turned off
 * @return
 */
double SitePercolation_ps_v8::spanningProbability() const {
    // TODO
    cout << "Not implemented : line " << __LINE__ << endl;
    return 0;
}


/**
 * Entropy calculation is performed here. The fastest method possible.
 * Cluster size is measured by bond.
 * @return current entropy of the lattice
 */
double SitePercolation_ps_v8::entropy() {
    double H{};
    double n = maxBonds() - _bonds_in_cluster_with_size_two_or_more;
//    cout << " _bonds_in_cluster_with_size_two_or_more " << _bonds_in_cluster_with_size_two_or_more << " : line " << __LINE__ << endl;
    H += n * log(1.0/double(maxBonds())) / double(maxBonds());
    H *= -1;
    _entropy_current =  _entropy_by_bond + H;
    return _entropy_current;
}


/**
 * number of bonds in the largest cluster / total number of bonds
 *
 * here cluster size is measured by number of bonds in the cluster
 * @return
 */
double SitePercolation_ps_v8::orderParameter() const{
    if(_number_of_bonds_in_the_largest_cluster != 0){
        // already calculated somewhere
        return double(_number_of_bonds_in_the_largest_cluster) / maxBonds();
    }
    value_type  len{}, nob{};
    for(const Cluster_v2& c: _clusters){
        nob = c.numberOfBonds();
        if (len < nob){
            len = nob;
        }
    }
    return double(len) / maxBonds();
}

/**
 * number of bonds in the largest cluster / total number of bonds
 *
 * here cluster size is measured by number of bonds in the cluster
 * @return
 */
double SitePercolation_ps_v8::orderParameter_v2() const{
//    double bond_num = _clusters[_index_largest_cluster].numberOfBonds() / double(_total_bonds);
    double tmp = _number_of_bonds_in_the_largest_cluster / double(maxBonds());
    return tmp;
}



/**
 * Finds out the number of bonds in the largest cluster by scanning all the clusters.
 * Best if no calculation is done for this previously
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInTheLargestCluster() {
    cout << "Inefficient. user *_v2() : line " << __LINE__ << endl;
    value_type  len{}, nob{};
    for(auto c: _clusters){
        nob = c.numberOfBonds();
        if (len < nob){
            len = nob;
        }
    }
    _number_of_bonds_in_the_largest_cluster = len;
    return len;
}


/**
 * Only applicable if the number of bonds in the largest cluster is calculated when occupying the lattice.
 * Significantly efficient than the previous version numberOfBondsInTheLargestCluster()
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInTheLargestCluster_v2() {
//    return _clusters[_index_largest_cluster].numberOfBonds();
    return _number_of_bonds_in_the_largest_cluster;
}


/**
 * Only applicable if the number of sites in the largest cluster is calculated when occupying the lattice.
 * Significantly efficient than the previous version numberOfBondsInTheLargestCluster()
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInTheSpanningCluster() {
    cout << "Not Implemented : line " << __LINE__ << endl;
    return _number_of_bonds_in_the_largest_cluster;
}


/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::numberOfSitesInTheLargestCluster() {
    value_type  len{}, nob{};
    for(auto c: _clusters){
        nob = c.numberOfSites();
        if (len < nob){
            len = nob;
        }
    }
    _number_of_sites_in_the_largest_cluster = len;
    return len;
}


/**********************************
 * Spanning methods
 **********************************/


/**
 *
 * @return
 */
double SitePercolation_ps_v8::numberOfSitesInTheSpanningClusters() {
    double nos{};
//    for(auto i: spanning_cluster_ids){
//        nos += _clusters[_cluster_index_from_id[i]].numberOfSites();
//    }

    int id{};
    for(auto i: _spanning_sites){
        id = _lattice.getSite(i).get_groupID();
        nos += _clusters[_cluster_index_from_id[id]].numberOfSites();
    }
    return nos;
}


/**
 *
 * @return
 */
double SitePercolation_ps_v8::numberOfBondsInTheSpanningClusters() {
    double nos{};
//    for(auto i: spanning_cluster_ids){
//        nos += _clusters[_cluster_index_from_id[i]].numberOfBonds();
//    }
    int id{};
    if(_spanning_sites.size() > 1){
        cout << endl;
        for(auto i: _spanning_sites){

            id = _lattice.getSite(i).get_groupID();
            cout << id << ", ";
            nos += _clusters[_cluster_index_from_id[id]].numberOfBonds();
        }
        cout << endl;
    }
    else{
        id = _lattice.getSite(_spanning_sites.front()).get_groupID();
//        cout << endl << "only one spanning cluster " << id << endl;
        nos = _clusters[_cluster_index_from_id[id]].numberOfBonds();
    }

    return nos;
}



/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::numberOfSitesInTheSpanningClusters_v2() {

    if(! _spanning_sites.empty()){
        int id = _lattice.getSite(_spanning_sites.front()).get_groupID();
        return _clusters[_cluster_index_from_id[id]].numberOfSites();
    }
    return 0;
}


/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInTheSpanningClusters_v2() {
    if(!_spanning_sites.empty()){
//        cout << "number of spanning sites " << _spanning_sites.size() << " : line " << __LINE__ << endl;
        int id = _lattice.getSite(_spanning_sites.front()).get_groupID();
        return _clusters[_cluster_index_from_id[id]].numberOfBonds();
    }
    return 0;
}

/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::numberOfSitesInTheWrappingClusters(){
    value_type nos{};
    int id{};
    for(auto i: _wrapping_sites){
        id = _lattice.getSite(i).get_groupID();
        nos += _clusters[_cluster_index_from_id[id]].numberOfSites();
    }
    return nos;
}

/**
 *
 * @return
 */
value_type SitePercolation_ps_v8::numberOfBondsInTheWrappingClusters(){
    value_type nos{};
    int id{};
    for(auto i: _wrapping_sites){
        id = _lattice.getSite(i).get_groupID();
        nos += _clusters[_cluster_index_from_id[id]].numberOfBonds();
    }
    return nos;
}


/**
 * _first_spanning_cluster_id must be positiove
 * @return
 */
int SitePercolation_ps_v8::birthTimeOfSpanningCluster() const {
    if(!_spanning_sites.empty()) {
        cout << "number of spanning sites " << _spanning_sites.size() << " : line " << __LINE__ << endl;
//        Index site = _spanning_sites.front();
//        int id = _lattice.getSite(site).set_groupID();
//        value_type clster_index = _cluster_index_from_id[id];
//        return _clusters[clster_index].birthTime();
        cout << "TODO : line " << __LINE__ << endl;
        return -1;
    }

    cout << "No spanning cluster : " << __LINE__ << endl;
    return -1;
}

/**
 * _first_spanning_cluster_id must be positiove
 * @return
 */
int SitePercolation_ps_v8::birthTimeOfACluster(int id) const {
    if (id < 0){
        cerr << "_first_spanning_cluster_id < 0 : line " << __LINE__ << endl;
        return -1;
    }
    for (const Cluster_v2& cls: _clusters){
        if(cls.get_ID() == id){
            return cls.birthTime();
        }
    }
    return -1;
}



/**
 * Box counting method for fractal dimension calculation
 * @param delta
 * @return
 */
value_type SitePercolation_ps_v8::box_counting(value_type delta) {
    value_type counter{};
    if(length() % delta == 0){
        for(value_type r{} ; r < length() ; r += delta){
            for(value_type c{}; c < length() ; c += delta){
                if(anyActiveSite(r, c, delta)){
                    ++ counter;
                }
            }
        }
    }
    else{
        cout << "Delta size is not corrent" << endl;
    }

    return counter;
}


/**
 * Box counting method for fractal dimension calculation
 * @param delta
 * @return
 */
array<value_type, 2> SitePercolation_ps_v8::box_counting_v2(value_type delta) {
    int spanning_cluster_id = _lattice.getSite(_spanning_sites.front()).get_groupID();
    value_type counter{}, spanning_counter{};
    bool foud_site{false}, found_spanning_site{false};
    int current_id{-1};
    if(length() % delta == 0){
        for(value_type r{} ; r < length() ; r += delta){
            for(value_type c{}; c < length() ; c += delta){
                for(value_type i{} ; i < delta ; ++i) {
                    for (value_type j{}; j < delta; ++j) {
                        current_id = _lattice.getSite({i + r, j + c}).get_groupID();
                        if(current_id >= 0){
                            foud_site = true;
                            if(current_id == spanning_cluster_id){
                                found_spanning_site = true;
                            }
                        }
                        if(foud_site && found_spanning_site){
                            break; // both are true no need to investigate further
                        }
                    }
                }

                if(foud_site){
                    ++ counter;
                    foud_site = false;
                }
                if(found_spanning_site){
                    ++spanning_counter;
                    found_spanning_site = false;
                }
            }
        }
    }
    else{
        cout << "Delta size is not corrent" << endl;
    }

    return array<value_type , 2>{counter, spanning_counter};
}




/**
 * Box counting method for fractal dimension calculation
 * @param delta
 * @return
 */
value_type SitePercolation_ps_v8::box_counting_spanning(value_type delta) {
    value_type counter{};
    if(length() % delta == 0){
        for(value_type r{} ; r < length() ; r += delta){
            for(value_type c{}; c < length() ; c += delta){
                if(anyActiveSpanningSite(r, c, delta)){
                    ++counter;
                }
            }
        }
    }
    else{
        cout << "Delta size is not corrent" << endl;
    }

    return counter;
}


/**
 * Return true if there is at least one active site in the region r, r + delta and c, c + delta
 * @param r
 * @param c
 * @param delta
 * @return
 */
bool SitePercolation_ps_v8::anyActiveSite(value_type row, value_type col, value_type delta) {
    for(value_type r{} ; r < delta ; ++r) {
        for (value_type c{}; c < delta; ++c) {
            if(_lattice.getSite({r+row, c + col}).isActive()){
                return true;
            }
        }
    }
    return false;
}


/**
 * Return true if there is at least one active site in the region r, r + delta and c, c + delta
 * Also this function required that, the site is in the spanning cluster
 * @param row
 * @param col
 * @param delta
 * @return
 */
bool SitePercolation_ps_v8::anyActiveSpanningSite(value_type row, value_type col, value_type delta) {
    int spanning_cluster_id = _lattice.getSite(_spanning_sites.front()).get_groupID();
    for(value_type r{} ; r < delta ; ++r) {
        for (value_type c{}; c < delta; ++c) {
            if(_lattice.getSite({r + row, c + col}).get_groupID() == spanning_cluster_id){
                return true;
            }
        }
    }
    return false;
}



/********************************
 *
 ********************************/

/**
 * Counts the number of active sites in the lattice
 */
value_type SitePercolation_ps_v8::count_number_of_active_site() {
    value_type counter{};
    for (value_type i{}; i != length(); ++i) {
        for (value_type j{}; j != length(); ++j) {
            if (_lattice.getSite({i, j}).isActive())
                ++counter;
        }
    }
    cout << "Number of active site : " << counter << endl;
    return counter;
}

value_type SitePercolation_ps_v8::placeSiteWeightedRelabeling_v9() {
    // randomly choose a site
    if (_index_sequence_position == maxSites()) {
        return ULONG_MAX;// unsigned long int maximum value
    }

    Index current_site = randomized_index_sequence[_index_sequence_position];
    _last_placed_site = current_site;
//    cout << "placing site " << current_site << endl;

    _lattice.activate_site(current_site);

    ++_number_of_occupied_sites;
    ++_index_sequence_position;

    // find the bonds for this site
    vector<BondIndex> bonds;
    vector<Index>     neighbors;
//    connection_v1(current_site, neighbors, bonds);
    connection_v2(current_site, neighbors, bonds);

    // find one of hv_bonds in _clusters and add ever other value to that place. then erase other position
    set<value_type> found_index_set = find_index_for_placing_new_bonds_v3(neighbors);

//    cout << "Found indices " << found_index_set << endl;

    subtract_entropy_for_bond(found_index_set);  // tracking entropy change
    value_type merged_cluster_index = manage_clusters_weighted_v8(
            found_index_set, bonds, current_site
    );
    add_entropy_for_bond(merged_cluster_index); // tracking entropy change

    // running tracker
    track_numberOfBondsInLargestCluster(); // tracking number of bonds in the largest cluster

    return merged_cluster_index;
}

void SitePercolation_ps_v8::logs() {
    if(_logging_flag){
        cout << "Number of relabeling " << _total_relabeling << endl;
    }
}

std::string SitePercolation_ps_v8::getSignature() {
    string s = "sq_lattice_site_percolation";
    if(_periodicity)
        s += "_periodic_";
    else
        s += "_non_periodic_";
    return s;
}

/**
 *
 * @param filename
 * @param only_spanning
 */
void SitePercolation_ps_v8::writeVisualLatticeData(const string &filename, bool only_spanning) {
    std::ofstream fout(filename);
    fout << "{\"length\":" << length() << "}" << endl;
//        fout << "#" << getSignature() << endl;
    fout << "#<x>,<y>,<color>" << endl;
    fout << "# color=0 -means-> unoccupied site" << endl;
    int id{-1};
    if(!_spanning_sites.empty()){
        id = _lattice.getSite(_spanning_sites.front()).get_groupID();
    }
    else if(!_wrapping_sites.empty()){
        id = _lattice.getSite(_wrapping_sites.front()).get_groupID();
    }

    if(only_spanning){
        vector<Index> sites = _clusters[_cluster_index_from_id[id]].getSiteIndices();
        for(auto s: sites){
            fout << s.column_ << ',' << s.row_ << ',' << id << endl;
        }
    }
    else {
        for (value_type y{}; y != length(); ++y) {
            for (value_type x{}; x != length(); ++x) {
                id = _lattice.getSite({y, x}).get_groupID();
                if(id != -1) {
                    fout << x << ',' << y << ',' << id << endl;
                }
            }
        }
    }
    fout.close();
}



/**
 * Simulate the program up to ensemble_size number of times in a single thread.
 *
 * @param ensemble_size
 * @param signature
 */
void SitePercolation_ps_v8::simulate_all(value_type ensemble_size) {
    cout << "Will be deleted in the future versions : line " << __LINE__ << endl;
//    size_t j{};
//    bool wrapping_occured {false};
//    _pcs.resize(ensemble_size);
//    _spanning_cluster_size_bonds.resize(ensemble_size);
//    _spanning_cluster_size_sites.resize(ensemble_size);
//
//    _occupation_probabilities = vector<double>(maxSites());
//
//    _nob_spanning = vector<double>(maxSites());
//    _nob_largest = vector<double>(maxSites());
//    _nos_largest = vector<double>(maxSites());
//    _nos_spanning = vector<double>(maxSites());
//
//    _entropy_sites = vector<double>(maxSites());
//    _entropy_bonds = vector<double>(maxSites());
//
//    std::mutex _mu_cout;
//    for(value_type i{} ; i != ensemble_size ; ++i){
//
//        reset();
//        j = 0;
//        wrapping_occured = false;
//        bool successful = false;
//        auto t_start = std::chrono::system_clock::now();
//        while (true){
//            successful = occupy();
//            if(successful) {
//                if(_periodicity) {
//                    if (!wrapping_occured && detectWrapping(lastPlacedSite())) {
//                        wrapping_occured = true;
//                        _pcs[i] = occupationProbability();
////                        _pcs[i] = _number_of_occupied_sites;
//                        _spanning_cluster_size_sites[i] = numberOfSitesInTheWrappingClusters();
//                        _spanning_cluster_size_bonds[i] = numberOfBondsInTheWrappingClusters();
//                    }
//                    if (wrapping_occured) {
//                        _nob_spanning[j] += numberOfBondsInTheWrappingClusters();
//                        _nos_spanning[j] += numberOfSitesInTheWrappingClusters();
//                    }
//                }else{
//                    if (!wrapping_occured && detectSpanning_v6(lastPlacedSite())) {
//                        wrapping_occured = true;
//                        _pcs[i] = occupationProbability();
////                        _pcs[i] = _number_of_occupied_sites;
//                        _spanning_cluster_size_sites[i] = numberOfSitesInTheSpanningClusters_v2();
//                        _spanning_cluster_size_bonds[i] = numberOfBondsInTheSpanningClusters_v2();
//
//                    }
//                    if (wrapping_occured) {
//                        _nob_spanning[j] += numberOfBondsInTheSpanningClusters_v2();
//                        _nos_spanning[j] += numberOfSitesInTheSpanningClusters_v2();
//                    }
//                }
//
//                _nob_largest[j] += numberOfBondsInTheLargestCluster_v2();
//                _nos_largest[j] += numberOfSitesInTheLargestCluster();
//
////            entrpy[j] += sp.entropy();  // old method and takes long time
//                _entropy_bonds[j] += entropy_v3(); // faster method
//                ++j;
//            }
//            if(j >= maxSites()){ // length_squared is the number of site
//                break;
//            }
//        }
//        {
//            auto t_end = std::chrono::system_clock::now();
//            std::lock_guard<mutex> lockGuard(_mu_cout);
//            cout << "Iteration " << i
//                 //<< " . Thread " << std::this_thread::get_id()
//                 << " . Elapsed time " << std::chrono::duration<double>(t_end - t_start).count() << " sec" << endl;
//        }
//    }
//
//    // Taking Average
//    for(size_t i{}; i!= maxSites() ; ++i){
//        _occupation_probabilities[i] = (i + 1) / double(maxSites());
//
//        _nob_spanning[i] /= double(ensemble_size);
//        _nob_largest[i]  /= double(ensemble_size);
//        _nos_largest[i]  /= double(ensemble_size);
//        _nos_spanning[i] /= double(ensemble_size);
//
//        _entropy_sites[i] /= double(ensemble_size);
//        _entropy_sites[i] /= double(ensemble_size);
//    }
}




/**
 *
 * @param ensemble_size
 */
void SitePercolation_ps_v8::simulate_periodic_critical(value_type ensemble_size) {
    cout << "Will be deleted in the future versions : line " << endl;
    if(!_periodicity) {
        cout << "Periodicity flag is False : file " << __FILE__ << " : line " << __LINE__ << endl;
    }
//
//    size_t j{};
//
//    _pcs.resize(ensemble_size);
//    _spanning_cluster_size_bonds.resize(ensemble_size);
//    _spanning_cluster_size_sites.resize(ensemble_size);
//
//    std::mutex _mu_cout;
//    for(value_type i{} ; i != ensemble_size ; ++i){
//
//        reset();
//        j = 0;
//
//        bool successful = false;
//        auto t_start = std::chrono::system_clock::now();
//        while (true){
//            successful = occupy();
//            if(successful) {
//
//                if (detectWrapping(lastPlacedSite())) {
//                    _pcs[i] = occupationProbability();
////                        _pcs[i] = _number_of_occupied_sites;
//                    _spanning_cluster_size_sites[i] = numberOfSitesInTheWrappingClusters();
//                    _spanning_cluster_size_bonds[i] = numberOfBondsInTheWrappingClusters();
//                    break;
//                }
//
//                ++j;
//            }
//            if(j >= maxSites()){ // length_squared is the number of site
//                break;
//            }
//        }
//        {
//            auto t_end = std::chrono::system_clock::now();
//            std::lock_guard<mutex> lockGuard(_mu_cout);
//            cout << "Iteration " << i
//                 //<< " . Thread " << std::this_thread::get_id()
//                 << " . Elapsed time " << std::chrono::duration<double>(t_end - t_start).count() << " sec" << endl;
//        }
//    }

}


//
//void SitePercolation_ps_v8::get_cluster_info(
//        vector<value_type> &site,
//        vector<value_type> &bond,
//        value_type &total_site,
//        value_type &total_bond
//) {
//
//    site.clear();
//    bond.clear();
//
//    unsigned long size = _clusters.size();
//    site.resize(size);
//    bond.resize(size);
//    total_site = 0;
//    total_bond = 0;
//    value_type a, b;
//    for(value_type i{}; i < size; ++i){
//        a = _clusters[i].numberOfSites();
//        b = _clusters[i].numberOfBonds();
//        site[i] = a;
//        bond[i] = b;
//        total_site += a;
//        total_bond += b;
//    }
//
//}



