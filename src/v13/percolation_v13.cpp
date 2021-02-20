//
// Created by shahnoor on 2/15/21.
//

#include <cmath>
#include <set>
#include "percolation_v13.h"

using namespace std;

void Percolation_v13::viewLattice(int formatt) {
        if (formatt < 0) {
            cout << "undefined format" << endl;
        }
        else if(formatt == 3) {
            lattice_ref.view_relative_index();
        }
        else if(formatt == 4) {
            lattice_ref.view_site_gids();
        }
        else {
            lattice_ref.view(formatt);
        }
}

RelativeIndex_v13 Percolation_v13::get_change_in_relative_index(RelativeIndex_v13 old_relative_index,
                                                                RelativeIndex_v13 new_relative_index) {
//# print("get_change_in_relative_index")
//# print("old_relative_index - new_relative_index = ", old_relative_index, " - ", new_relative_index)
    auto change = new_relative_index - old_relative_index;
//# print("get_change_in_relative_index. type of change ", type(change))
//# print("change ", change)
    return RelativeIndex_v13(change);
}

vector<int> Percolation_v13::get_bond_gids(std::vector<int> &bond_ids) {
    set<int> gids;
    for (auto bb : bond_ids) {
        int gid = lattice_ref.get_bond_by_id(bb).get_gid();
        gids.insert(gid);
    }
    vector<int> vec(gids.begin(), gids.end());
    return vec;
}

vector<int> Percolation_v13::get_site_gids(std::vector<int> &site_ids) {
    set<int> gids;
    for (auto ss : site_ids) {
        int gid = lattice_ref.get_site_by_id(ss).get_gid();
//        cout << "site " << ss << " gid => " << gid << endl;
        gids.insert(gid);
    }
    vector<int> vec(gids.begin(), gids.end());
    return vec;
}

/**
 * when delta X of relative indices are greater than 1 and there are in
    the opposite edge of the lattice.

 * @param delta_X
 * @return
 */
RelativeIndex_v13 Percolation_v13::wrapping_correction_relative_index(RelativeIndex_v13 delta_X) {
//# LL = self.lattice_ref.length
    int xx = delta_X.x_coord();
    int yy = delta_X.y_coord();
    if (abs(xx) > 1) {
        xx = -xx / abs(xx);
    }
    if (abs(yy) > 1) {
        yy = -yy / abs(yy);
    }
//# print(type(xx), " and ", type(yy))
    return RelativeIndex_v13(xx, yy);
}

Index_v13 Percolation_v13::wrapping_correction_relative_index(Index_v13 delta_X) {
//# LL = self.lattice_ref.length
    int xx = delta_X.row();
    int yy = delta_X.col();
    if (abs(xx) > 1) {
        xx = -xx / abs(xx);
    }
    if (abs(yy) > 1) {
        yy = -yy / abs(yy);
    }
//# print(type(xx), " and ", type(yy))
    return Index_v13(xx, yy);
}

/**
 * neighbor_site_id will get a new relative index based on central_site_id. only one condition,
                new site must be a neighbor of old site

 * @param central_site_id
 * @param neighbor_site_id
 * @return
 */
RelativeIndex_v13 Percolation_v13::get_relative_index(int central_site_id, int neighbor_site_id) {
//# print("central_site_id ", central_site_id)
//# print("neighbor_site_id ", neighbor_site_id)
    Index_v13 &central_index = lattice_ref.get_site_by_id(central_site_id).get_index();
    Index_v13 &neighbor_index = lattice_ref.get_site_by_id(neighbor_site_id).get_index();
//# print("central_index ", central_index)
//# print("neighbor_index ", neighbor_index)
    Index_v13 idx = neighbor_index.subtract(central_index);
//# print("idx ", idx)
    idx = wrapping_correction_relative_index(idx);
//# print("after wrapping correction ", idx)
    Index_v13 old_relative_index = lattice_ref.get_site_by_id(central_site_id).get_relative_index();
//# new_relative_index = self.lattice_ref.get_site_by_id(new_site_id).get_relative_index()
//# print(old_relative_index, " old_relative_index type ", type(old_relative_index))
    auto new_relative_index = old_relative_index + idx;
//# print("new_relative_index type ", type(new_relative_index))
//# print("new_relative_index type ", type(RelativeIndex(index=new_relative_index)))
    return RelativeIndex_v13(new_relative_index);
}

SitePercolation_v13::SitePercolation_v13(int length, int seed) : Percolation_v13(length, seed) {

    init_clusters();

    site_ids_indices = lattice_ref.get_site_id_list();
    int current_idx = 0;
    shuffle_indices();
    selected_id = -1;
    cluster_count = lattice_ref.get_bond_count();
    largest_cluster_sz = 0;
    largest_cluster_id = -1;
    max_entropy = log(lattice_ref.get_bond_count());
    entropy_value = max_entropy;
    after_wrapping = false;
    wrapping_cluster_id = -1;
}

void SitePercolation_v13::init_clusters() {
    cluster_pool_ref.reset();
    for (auto bb: lattice_ref.get_bond_id_list()){
        cluster_pool_ref.create_new_cluster(-1, bb, lattice_ref);
    }
}

void SitePercolation_v13::reset() {
    Percolation_v13::reset();

    init_clusters();
    shuffle_indices();
    current_idx = 0;
    after_wrapping = false;
    wrapping_cluster_id = -1;
    largest_cluster_sz = 0;
    largest_cluster_id = -1;
    entropy_value = max_entropy;
//# print("Initial entropy ", self.entropy_value)
}

double SitePercolation_v13::entropy_v1() {
    double H = 0;
    for(int i=0; i < cluster_count;++i){
        int b_count = cluster_pool_ref.get_cluster_bond_count(i);
        long mu = b_count / lattice_ref.get_bond_count();
        if (mu==0) continue;
        double log_mu = log(mu);
        H += mu * log_mu;
    }

    return -H;
}

double SitePercolation_v13::order_param_wrapping() {
    if (after_wrapping) {
//# print("wrapping cluster id ", self.wrapping_cluster_id)
        double count = cluster_pool_ref.get_cluster_bond_count(wrapping_cluster_id);
        return count / lattice_ref.get_bond_count();
    }
    return 0;
}

int SitePercolation_v13::get_neighbor_site(int central_id, int connecting_bond_id) {
//# central_id = current_site.get_id()
//# print("central site id : ", central_id)

    auto connected_sites = lattice_ref.get_neighbor_sites(connecting_bond_id);
//# print("connected ", connected_sites)
//    for (int i=0; i < connected_sites.size(); ++i){
//        if (connected_sites[i] == central_id){
//            connected_sites.erase(connected_sites.begin() + i);
//            break;
//        }
//    }
    remove_vector_element(connected_sites, central_id);
    if(connected_sites.size() != 1){
        cout << "Number of neighbors must be exactly 1 : get_connected_sites()" << endl;
    }

//# print("neighbor site ids ", neighbor_sites)
    return connected_sites[0];
//    pass
}

std::vector<int> SitePercolation_v13::get_connected_sites(Site_v13 site, std::vector<int> &bond_neighbors) {
//# print("central site index : ", site.get_index())
    auto central_id = site.get_id();
//# print("central site id : ", central_id)
    vector<int> neighbor_sites;
    for(auto bb : bond_neighbors) {
        auto sb2 = lattice_ref.get_bond_by_id(bb);
        auto connected_sites = sb2.get_connected_sites();
//# print("connected ", connected_sites)
        remove_vector_element(connected_sites, central_id);
        if (connected_sites.size() > 1) {
            cout << "Number of neighbors cannot exceed 2 : get_connected_sites()" << endl;
        }
        neighbor_sites.push_back(connected_sites[0]);
    }
//# print("neighbor site ids ", neighbor_sites)
    return neighbor_sites;
}

bool SitePercolation_v13::select_site() {

    if (current_idx >= lattice_ref.get_site_count()) {
//# print("No sites to occupy")
        return false;
    }
    selected_id = site_ids_indices[current_idx];
    current_site = lattice_ref.get_site_by_id(selected_id);
//# print("selected id ", selected_id)
    return true;

}

bool SitePercolation_v13::place_one_site() {
    cout << "************************ place_one_site. count " << current_idx << endl;
    auto flag = select_site();
    if(flag) {

//        cout << "selected site ", self.current_site.get_index(), " id ", self.current_site.get_id())
        lattice_ref.init_relative_index(selected_id);  // initialize        relative index
        auto bond_neighbors = current_site.get_connecting_bonds();
//# site_neighbors = self.get_connected_sites(self.current_site, bond_neighbors)

        entropy_subtract(bond_neighbors);

        auto merged_cluster_index = merge_clusters_v2(bond_neighbors);

        track_largest_cluster(merged_cluster_index);
        entropy_add(merged_cluster_index);

//# self.lattice_ref.set_site_gid_by_id(selected_id, merged_cluster_index)
//# self.cluster_pool_ref.add_sites(merged_cluster_index, selected_id)
        current_idx += 1;
    }
    return flag;
}

void SitePercolation_v13::track_largest_cluster(int new_cluster) {
    auto new_size = cluster_pool_ref.get_cluster_bond_count(new_cluster);
//# self.cluster_pool_ref.get_cluster_site_count(new_cluster)
    if (new_size > largest_cluster_sz) {
        largest_cluster_id = new_cluster;
        largest_cluster_sz = new_size;
    }
}

void SitePercolation_v13::entropy_subtract(std::vector<int> &bond_neighbors) {
//# print("entropy_subtract")
    auto bonds = lattice_ref.get_neighbor_bonds(selected_id);
//# print(self.current_site, " neighbors => ", sites)
    set<int> gids;
    for(auto bb: bonds) {
        auto gid = lattice_ref.get_bond_gid_by_id(bb);
        if (gid == -1)        continue;
        gids.insert(gid);
    }
//# print("gids ", gids)
    double H = 0, mu;
    double bc = lattice_ref.get_bond_count();
    for (auto gg : gids) {
        auto b_count = cluster_pool_ref.get_cluster_bond_count(gg);
        if (b_count == 0)        continue;

        mu = b_count / bc;
        H += mu * log(mu);
    }
//# print("before ", self.entropy_value)
    entropy_value += H;
//# print("after ", self.entropy_value)

}

void SitePercolation_v13::entropy_add(int new_cluster_id) {
//# print("entropy_add")
    double b_count = cluster_pool_ref.get_cluster_bond_count(new_cluster_id);
    double mu = b_count / lattice_ref.get_bond_count();
//# print("before ", self.entropy_value)
    entropy_value -= mu*log(mu);
//# print("after ", self.entropy_value)
}

double SitePercolation_v13::entropy() {
//    return entropy_v1();
    return entropy_v2();
}

double SitePercolation_v13::entropy_v2() {
    return entropy_value;
}

void SitePercolation_v13::relabel_relative_indices(int connecting_bond) {
    auto bond = lattice_ref.get_bond_by_id(connecting_bond);
    auto bbg = bond.get_gid();
    auto central_site = current_site.get_id();
    auto neighbor_site = get_neighbor_site(central_site, bond.get_id());
//# print("neighbor_site ", neighbor_site)
    auto sites_to_relabel = cluster_pool_ref.get_sites(bbg);
//# print("sites_to_relabel ", sites_to_relabel)
//# relabel neighbor according to central site
    if (sites_to_relabel.size() == 0) {
//# print("len(sites_to_relabel) == 0 ")
        return;
    }
    auto old_relative_idx = lattice_ref.get_site_by_id(neighbor_site).get_relative_index();
    auto new_relative_idx = get_relative_index(central_site, neighbor_site);
//# if the BBB lines are commented then it sould not affect the result. so why extra lines;
//    # self.lattice_ref.set_relative_index(neighbor_site, new_relative_idx)  # BBB
//        # then relabel all sites belonging to the cluster according to the neighbor

    if (lattice_ref.get_site_gid_by_id(central_site) >= 0){
//# old_relative_index = self.get_relative_index(central_site, self.selected_id)
        auto change = get_change_in_relative_index(old_relative_idx, new_relative_idx);
        RelativeIndex_v13 changeR = RelativeIndex_v13(change);
//# print("change ", change)
//# print("old_relative_index ", old_relative_idx)
        for (auto ss : sites_to_relabel) {
//# if ss == neighbor_site:  # BBB
//#     print("already got relabeled") # BBB
//#     continue # BBB
            RelativeIndex_v13 ss_relative_index = lattice_ref.get_site_by_id(ss).get_relative_index();
//# change = self.get_change_in_relative_index(old_relative_index, ss_relative_index)
//# print("change ", change)
//# print("new_relative_index type ", type(ss_relative_index))
//# print("relative index before : ", ss_relative_index)
            auto temp = ss_relative_index + changeR;
            ss_relative_index = RelativeIndex_v13(temp);
//# print("relative index after  : ", ss_relative_index)
//# print("new_relative_index ", new_relative_index)
            lattice_ref.get_site_by_id(ss).set_relative_index(ss_relative_index);
        }
    }
}

double SitePercolation_v13::occupation_prob() {
    return double(current_idx) / lattice_ref.get_site_count();
}

int SitePercolation_v13::merge_clusters(std::vector<int> &bond_neighbors) {
    auto bond_gids = get_bond_gids(bond_neighbors);
//# print("merging clusters ", bond_gids)
    int ref_sz = 0,    root_clstr = 0;
    for (auto bb : bond_gids) {
        int sz = cluster_pool_ref.get_cluster_bond_count(bb);
        if (sz >= ref_sz) {
            root_clstr = bb;
            ref_sz = sz;
        }
    }
    cout << "root cluster is " << root_clstr << endl;
    for (auto bb : bond_gids) {
        if (bb == root_clstr) {
        //# print("bb ", bb, " is a root cluster")
            continue;
        }
    cout << "merging " << bb << " to " << root_clstr << endl;
        cluster_pool_ref.merge_cluster_with(root_clstr, bb, lattice_ref);
    }

    return root_clstr;
}

/**
 * merging with relabeling relative indices
 * @param bond_neighbor
 * @return
 */
int SitePercolation_v13::merge_clusters_v2(std::vector<int> &bond_neighbor) {

    auto bond_neighbors = uniqe_gid_bond_neighbors(bond_neighbor);
    auto bond_gids = get_bond_gids(bond_neighbors);
//# site_gids = self.get_site_gids(site_neighbors)
//# print("site_gids ", site_gids)
//# print("bond_gids ", bond_gids)
//# print("set minus ", set(site_gids) - set(bond_gids))
//# print("merging clusters ", bond_gids)
    int ref_sz = 0,    root_clstr = bond_gids[0];
    for (auto bbg : bond_gids) {
        int sz = cluster_pool_ref.get_cluster_bond_count(bbg);
        if (sz >= ref_sz) {
            root_clstr = bbg;
            ref_sz = sz;
        }
    }

//# print("root cluster is ", root_clstr)
//# print("Assign and relabel currently selected site")
    for (auto bb : bond_neighbors) {
        int bbg = lattice_ref.get_bond_by_id(bb).get_gid();
        if (bbg == root_clstr) {
//# print("bb ", bbg, " is a root cluster")
//# relabel and assign the current site here
            lattice_ref.set_site_gid_by_id(selected_id, root_clstr);
            cluster_pool_ref.add_sites(root_clstr, {selected_id});
//# relabeling current site. relative index
            auto neighbor_site = get_neighbor_site(current_site.get_id(), bb);
            if (lattice_ref.get_site_gid_by_id(neighbor_site) >= 0) {
//# relabel selected site with respect to neighbor site. so neighbor_site is the central site
                auto rri = get_relative_index(neighbor_site, selected_id);
//# rri = self.get_relative_index(self.selected_id, neighbor_site)

//# sitttte = self.lattice_ref.get_site_by_id(self.selected_id)
//# print("relative index before ", sitttte.get_relative_index())
//# print(self.selected_id, " ", sitttte, " => rri ", rri)

                lattice_ref.set_relative_index(selected_id, rri);
            }else{
                cout << "Does not belong to any cluster yet" << endl;
            }
        }
    }


//# print("Relabel all cluster according to root cluster and selected site")
    for (auto bb : bond_neighbors) {
        int bbg = lattice_ref.get_bond_by_id(bb).get_gid();
        if (bbg == root_clstr)        continue;
//# print("relabeling relative index of cluster ", bbg)
        relabel_relative_indices(bb);
//# print("merging ", bbg, " to ", root_clstr)
        cluster_pool_ref.merge_cluster_with(root_clstr, bbg, lattice_ref);
    }

    for (int bbg : bond_gids) {
        if (bbg == root_clstr) {
            cout << "bb " << bbg << " is a root cluster" << endl;
            continue;
        }
        cluster_pool_ref.clear_cluster(bbg);
    }
    return root_clstr;

}

std::vector<int> SitePercolation_v13::uniqe_gid_bond_neighbors(std::vector<int> &bond_neighbors) {
    vector<int> gids, unique_bond_ids;
    for (int bb : bond_neighbors) {
        int bbg = lattice_ref.get_bond_by_id(bb).get_gid();
        if (find_elm(gids, bbg))  continue;
        else {
            gids.push_back(bbg);
            unique_bond_ids.push_back(bb);
        }
    }
    return unique_bond_ids;

}

bool SitePercolation_v13::detect_wrapping() {
    if (after_wrapping) return true;

//# print("detect_wrapping")
    auto neighbors = lattice_ref.get_sites_for_wrapping_test(selected_id);
//# print("neighbors of self.selected_id with same gid : ", neighbors)
    auto central_r_index = current_site.get_relative_index();
    for (auto ss : neighbors) {
        auto rss = lattice_ref.get_site_by_id(ss).get_relative_index();
        auto delta_x = central_r_index.x_coord() - rss.x_coord();
        auto delta_y = central_r_index.y_coord() - rss.y_coord();
        if ((abs(delta_x) > 1) or (abs(delta_y) > 1)) {
//# print(self.selected_id, " and ", ss, " are connected via wrapping")
//# print("indices are ", self.lattice_ref.get_site_by_id(self.selected_id).get_index(),
//#       " and ", self.lattice_ref.get_site_by_id(ss).get_index())
//# print("relative ", central_r_index, " - ", rss)
            after_wrapping = true;
            wrapping_cluster_id = lattice_ref.get_site_by_id(selected_id).get_gid();
            return true;
        }
    }
    return false;
}

void SitePercolationL0_v13::reset() {
    SitePercolation_v13::reset();
    int ll = get_length();
    int l_squared = ll*ll;

    if (first_run) {
        occupation_prob_list.clear();
        occupation_prob_list.resize(l_squared);
    }

    entropy_list.clear();
    order_wrapping_list.clear();
    order_largest_list.clear();


    entropy_list.resize(l_squared);
    order_wrapping_list.resize(l_squared);
    order_largest_list.resize(l_squared);
}

SitePercolationL0_v13::SitePercolationL0_v13(int length, int seed) : SitePercolation_v13(length, seed) {
    first_run = true;

    int ll = get_length();
    int l_squared = ll*ll;
    occupation_prob_list.resize(l_squared);
    entropy_list.resize(l_squared);
    order_wrapping_list.resize(l_squared);
    order_largest_list.resize(l_squared);
}

void SitePercolationL0_v13::run_once() {
//# sq_lattice_p.viewLattice(3)
//# sq_lattice_p.viewCluster()
    double p, H, P1, P2;

    while (place_one_site()) {
        detect_wrapping();
        if (first_run) {
            p = occupation_prob();
            occupation_prob_list.push_back(p);
        }
        H = entropy();
        P1 = order_param_wrapping();
        P2 = order_param_largest_clstr();

        entropy_list.push_back(H);
        order_wrapping_list.push_back(P1);
        order_largest_list.push_back(P2);
    }

    first_run = false;
}
