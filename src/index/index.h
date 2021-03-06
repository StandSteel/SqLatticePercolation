//
// Created by shahnoor on 10/2/2017.
//

#ifndef SITEPERCOLATION_INDEX_H
#define SITEPERCOLATION_INDEX_H


#include <ostream>
#include <iostream>
#include <sstream>
#include <vector>

#include "../types.h"
#include "../exception/exceptions.h"
#include "../lattice/bond_type.h"


/**
 *
 */
struct Index{
    value_type row_{};
    value_type column_{};

    ~Index()                      = default;
    Index()                       = default;

//    Index(Index&)               = default;
//    Index(Index&&)              = default;
//    Index& operator=(Index&)    = default;
//    Index& operator=(Index&&)   = default;

    Index(value_type x, value_type y) : row_{x}, column_{y} {}

};


/**
 * Template index
 * @tparam T
 */
class IndexRelative{
public:
    int x_{};
    int y_{};

    ~IndexRelative()                      = default;
    IndexRelative()                       = default;

    IndexRelative(int x, int y) : x_{x}, y_{y} {}

};

/**
 *
 */
struct BondIndex{
    BondType bondType;

    value_type row_;
    value_type column_;

    ~BondIndex()                        = default;
    BondIndex()                         = default;
//    BondIndex(BondIndex&)               = default;
//    BondIndex(BondIndex&&)              = default;
//    BondIndex& operator=(BondIndex&)    = default;
//    BondIndex& operator=(BondIndex&&)   = default;


    BondIndex(BondType hv, value_type row, value_type column)
            :  row_{row}, column_{column}
    {
        bondType = hv;
    }

    bool horizontal() const { return bondType == BondType::Horizontal;}
    bool vertical() const { return bondType == BondType::Vertical;}

};

/**
* This is a translator class that translate
*      (a) 1D index to 2D and vice versa for Index.
*          (r,c) = r*L + c = index_site
*      (b) 1D index to 2D BondIndex and vice versa.
*          (t,r,c) = t*L^2 + r*L + c = index_bond
*
*      here, r -> row index
*            c -> column index
*            t -> bond type. 0 if horizontal and 1 if vertical
**/
class IndexTranslator{
public:
    static index_type translateBondTo1D(size_t length, BondIndex bondIndex);
    static BondIndex translate1DToBond(size_t length, index_type index);
    static index_type translateSiteTo1D(size_t length, Index index);
    static Index translate1DToSite(size_t length, index_type index);

};

std::ostream& operator<<(std::ostream& os, const Index& index);
bool operator==(const Index& index1, const Index& index2);
bool operator<(const Index& index1, const Index& index2);

std::ostream& operator<<(std::ostream& os, const IndexRelative& index);

std::ostream& operator<<(std::ostream& os, const BondIndex& index);
bool operator==(BondIndex index1, BondIndex index2);
bool operator<(BondIndex index1, BondIndex index2);


/**
 *  Get 2nd nearest neightbor / sin the direction of 1st nearest neighbor, while @var center is the center
 */
Index get_2nn_in_1nn_direction(Index center, Index nn_1, value_type length);
std::vector<Index> get_all_2nn_in_1nn_s_direction(Index center, const std::vector<Index> &nn_1, value_type length);

#endif //SITEPERCOLATION_INDEX_H
