//
// Created by shahnoor on 11/22/19.
//

#include <algorithm>
#include "percolation_v12.h"

using namespace std;

/**
 *
 * @param length
 */
Percolation_v12::Percolation_v12(int length):_length{length} {
    if(_length < 2)
        _length = 2;
    _lattice = Lattice_v12(length);


    _max_number_of_sites = static_cast<size_t>(_length * _length);
    _max_number_of_bonds = 2*_max_number_of_sites;
}


void Percolation_v12::setRandomState(size_t seed, bool generate_seed) {
//    size_t seed = 0;
//    cerr << "automatic seeding is commented : line " << __LINE__ << endl;
    _random_state = seed;
    if(generate_seed) {
        std::random_device _rd;
        _random_state = _rd();
    }else{
        cerr << "generate_seed = false : line " << __LINE__ << endl;
    }
    _random.seed(_random_state); // seeding
    cout << "seeding with " << _random_state << endl;
}


size_t Percolation_v12::getRandomState() {
//    size_t seed = 0;
//    cerr << "automatic seeding is commented : line " << __LINE__ << endl;
//    std::random_device _rd;
//    auto seed = _rd();
//    _random.seed(seed); // seeding
//    cout << "seeding with " << seed << endl;
    return _random_state;
}

void Percolation_v12::viewCluster() {
    cout << "Percolation_v12::viewCluster" << endl;
    for(size_t i{}; i < _clusters.size(); ++i){
        if(_clusters[i].empty()) continue;
        cout << "cluster[" << i << "] {" << endl;

        auto bonds = _clusters[i].getBondIDs();
        auto sites = _clusters[i].getSiteIDs();
        cout << "  Sites (" << sites.size()  << ") : {";
        for(auto s: sites){
            cout << s << ",";
        }
        cout << "}" << endl;
        cout << "  Bonds (" << bonds.size()  << ") : {";
        for(auto s: bonds){
            cout << s << ",";
        }
        cout << "}" << endl;
        cout << "}" << endl;
    }
}


SqLatticeRegularSite::SqLatticeRegularSite(int length) : Percolation_v12(length) {
    auto sites = _lattice.getSites();
    for(auto s: sites) {
        auto id = _lattice.getSiteID(s);
        index_sequence.emplace_back(id);
    }

}

void SqLatticeRegularSite::init() {
    //
    _entropy = logl(maxBonds());

    // activate bonds and initialize cluster
    _clusters.resize(maxBonds());
    auto bonds = _lattice.getBonds();
    for(int i{}; i < _clusters.size(); ++i){

        auto id = _lattice.getBondID(bonds[i]);
        _lattice.activateBond(bonds[i]);
        _lattice.setGroupIDBond(bonds[i], i);
        _clusters[i].addBond(id);
        _clusters[i].setGroupID(i);
    }

    randomized_index = index_sequence;

    std::shuffle(randomized_index .begin(), randomized_index.end(), _random);

}

bool SqLatticeRegularSite::occupy() {
    if(index_counter >= randomized_index.size()) return false;
    // select a site randomly
//    size_t i = _random() % randomized_index.size();
    id_last_site = randomized_index[index_counter];
    index_counter++;

    // activate the site
    _lattice.activateSite(id_last_site);
    ++_number_of_occupied_sites;

    // find it's neighbors. sites and then bonds
    auto bond_connected = _lattice.getSite(id_last_site).connectedBondIDs();
    // find which clusters the bonds belong
    // find a suitble root cluster
    int root{-1};
    size_t root_size{}, tmp{};
    set<int> gids; // to prevent repetition
    for(auto b: bond_connected){
        auto gid = _lattice.getGroupIDBond(b);
        gids.emplace(gid);
        tmp = _clusters[gid].numberOfSites();
        if(tmp >= root_size){
            root_size = tmp;
            root = gid;
        }
    }

    _lattice.setGroupIDSite(id_last_site, root);
    _clusters[root].addSite(id_last_site);
    // insert all to it
    for(auto g: gids){
        if(g == root) continue;
        cout << "g = " << g << endl;
        _clusters[root].insert(_clusters[g]);

        // relabel
        relabel(_clusters[g], id_last_site);

        _clusters[g].clear();
    }



    return true;
}

/**
 * Relabel all sites and bonds of `clstr` cluster
 * @param clstr
 * @param id_current : id of the current site
 */
void SqLatticeRegularSite::relabel(Cluster_v12 &clstr, int id_current) {
    int gid_current = _lattice.getGroupIDSite(id_current);
    auto site = _lattice.getSite(id_current);
    auto coordinate_new = site.get_index();
    auto relative_new = site.relativeIndex();
    cout << "relative index of new site " << relative_new << endl;
    auto bonds = clstr.getBondIDs();
    for(auto b: bonds){
        // relabel bond group id
        _lattice.setGroupIDBond(b, gid_current);
    }

    // calculate relative index and relabel sites
    auto sites_neighbor = _lattice.get_neighbor_sites_of_site(id_current);
    // if any of these neighboring sites belong to the same cluster that current site does then
    // we can use that site's relative index and coordinate index to relabel the relative index of merging cluster
    Index coordinate_old ;
    IndexRelative relative_old;

    cout << "gid of current " << gid_current << endl;
    for(auto s: sites_neighbor){
        auto gid = _lattice.getGroupIDSite(s);
        cout <<"neighbor =" << s << "  gid =" << gid << endl;
        if(gid == gid_current){
            relative_old = _lattice.getRelativeIndex(s);
            coordinate_old = s;
            cout << "relative index of old site " << relative_old << endl;
            cout << "coordinate index of old site " << coordinate_old << endl;
            break;
        }
    }

    int dx_r = relative_old.x_ - relative_new.x_;
    int dy_r = relative_old.y_ - relative_new.y_;

    int dx_c = int(coordinate_new.column_) - int(coordinate_old.column_); // since it can be negative
    int dy_c = int(coordinate_new.row_)- int(coordinate_old.row_);

    int dx = dx_r + dx_c;
    int dy = dy_r + dy_c;

    auto sites = clstr.getSiteIDs();
    for(auto s: sites){
        // relabel site group id
        _lattice.setGroupIDSite(s, gid_current);
        // relabel relative index
        _lattice.getSite(s).addToRelativeIndex(dx, dy);
    }

}

void SqLatticeRegularSite::reset() {
    index_counter = 0;


    init();
}


