//
// Created by shahnoor on 6/28/21.
//

#include "percolation_rsbd_v13.h"
#include "status.h"

using namespace std;

/**
 * return :
 *  0  -> successfully chosen an empty site
 *  1  -> sites are remaining but current site is not empty
 *  -1 -> no remaining empty sites
 * @return
 */
P_STATUS SitePercolationL1_v13::select_site() {

    if (current_idx >= lattice_ref.get_site_count()) {
        cout << "No sites to occupy" << endl;
        return P_STATUS::EMPTY_SITE_LIST;
    }
    value_type rnd = current_idx + _random_engine() % site_ids_indices.size();

    auto central_X = site_ids_indices[rnd];

    if(lattice_ref.get_site_by_id(central_X).is_occupied()){
        cout << "X is occupied" << endl;
        ++x_occupied;

        auto sites = lattice_ref.get_all_neighbor_sites(central_X);


        auto central2 = sites[_random_engine()% sites.size()];

        if (lattice_ref.get_site_by_id(central2).is_occupied()){
            lattice_ref.get_site_by_id(central_X).reduce_1st_nn();
            if (lattice_ref.get_site_by_id(central_X).is_removable(1)){
//# print("is_removable")
//# print("self.site_ids_indices before ", self.site_ids_indices)
//# print("rnd ", rnd, " self.current_idx ", self.current_idx)

                site_ids_indices[rnd] = site_ids_indices[current_idx];

                current_idx += 1;
            }
            return P_STATUS::CURRENT_SITE_NOT_EMPTY;
        }

    }


//    current_site = lattice_ref.get_site_by_id(selected_id);
//# print("selected id ", self.selected_id)
    occupied_site_count += 1;

//    current_site = lattice_ref.get_site_by_id(selected_id);
//    cout << "selected id " << selected_id << " site " << current_site.get_str() << endl;
//    cout << "selected id " << selected_id << " site " << get_current_site().get_str() << endl;
    return P_STATUS::SUCESS;





}

