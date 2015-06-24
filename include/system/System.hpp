#pragma once
#include "support/dense.hpp"
#include "support/sparse.hpp"
#include <vector>

namespace tbm {

class Lattice;
class Shape;
class Foundation;
class Symmetry;
class SystemModifiers;

/**
 Stores the positions and base hoppings for all lattice sites.
 */
class System {
public:
    /// Stores sites that belong to a boundary
    struct Boundary {
        Boundary(const System* system) : system(system) {}

        std::tuple<Cartesian, Cartesian> get_position_pair(int i, int j) const {
            return std::make_tuple(system->positions[i], system->positions[j] - shift);
        }
        
        const System* system;
        Cartesian shift; ///< shift length (periodic boundary condition)
        SparseMatrixX<float> matrix;
        int max_elements_per_site = 0;
    };

public:
    System(const Lattice& lattice, const Shape& shape,
           const Symmetry* symmetry, const SystemModifiers& system_modifers);

    void build_from(Foundation& foundation);
    void build_boundaries_from(Foundation& foundation, const Symmetry& symmetry);

public:
    /// Find the index of the site nearest to the given position. Optional: filter by sublattice.
    int find_nearest(const Cartesian& position, short sublattice = -1) const;

    int num_sites() const { return positions.size(); }
    std::tuple<Cartesian, Cartesian> get_position_pair(int i, int j) const {
        return std::make_tuple(positions[i], positions[j]);
    }

public:
    CartesianArray positions; ///< coordinates of all the lattice sites
    ArrayX<short> sublattice; ///< sublattice indices of all the sites
    SparseMatrixX<float> matrix; ///< base hopping information
    std::vector<Boundary> boundaries; ///< boundary information

    int max_elements_per_site; ///< maximum number of Hamiltonian element at any site
    std::string report;
};

} // namespace tbm
