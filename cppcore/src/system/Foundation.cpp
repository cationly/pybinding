#include "system/Foundation.hpp"
#include "system/Shape.hpp"

namespace cpb { namespace detail {

std::pair<Index3D, Index3D> find_bounds(Shape const& shape, Lattice const& lattice) {
    Array3i lower_bound = Array3i::Constant(std::numeric_limits<int>::max());
    Array3i upper_bound = Array3i::Constant(std::numeric_limits<int>::min());
    for (auto const& point : shape.vertices) {
        // Translate Cartesian coordinates `point` into lattice vector coordinates `v`
        Array3i const v = lattice.translate_coordinates(point).cast<int>();
        lower_bound = (v < lower_bound).select(v, lower_bound);
        upper_bound = (v > upper_bound).select(v, upper_bound);
    }

    // Add +/- 1 padding to compensate for `cast<int>()` truncation
    auto const ndim = lattice.ndim();
    lower_bound.head(ndim) -= 1;
    upper_bound.head(ndim) += 1;

    return {lower_bound, upper_bound};
}

CartesianArray generate_positions(Cartesian origin, Index3D size, Lattice const& lattice) {
    // The nested loops look messy, but it's the fastest way to calculate all the positions
    // because the intermediate a, b, c positions are reused.
    auto const nsub = lattice.nsub();
    auto const num_sites = size.prod() * nsub;
    CartesianArray positions(num_sites);

    auto idx = 0;
    for (auto s = 0; s < nsub; ++s) {
        Cartesian ps = origin + lattice[s].position;
        for (auto c = 0; c < size[2]; ++c) {
            Cartesian pc = (c == 0) ? ps : ps + static_cast<float>(c) * lattice.vector(2);
            for (auto b = 0; b < size[1]; ++b) {
                Cartesian pb = (b == 0) ? pc : pc + static_cast<float>(b) * lattice.vector(1);
                for (auto a = 0; a < size[0]; ++a) {
                    Cartesian pa = pb + static_cast<float>(a) * lattice.vector(0);
                    positions[idx++] = pa;
                } // a
            } // b
        } // c
    } // sub

    return positions;
}

ArrayX<int16_t> count_neighbors(Foundation const& foundation) {
    ArrayX<int16_t> neighbor_count(foundation.get_num_sites());

    auto const& lattice = foundation.get_lattice();
    auto const size = foundation.get_size().array();

    for (auto const& site : foundation) {
        auto const& sublattice = lattice[site.get_sublattice()];
        auto num_neighbors = static_cast<int16_t>(sublattice.hoppings.size());

        // Reduce the neighbor count for sites on the edges
        for (auto const& hopping : sublattice.hoppings) {
            auto const index = (site.get_index() + hopping.relative_index).array();
            if (any_of(index < 0) || any_of(index >= size))
                num_neighbors -= 1;
        }

        neighbor_count[site.get_idx()] = num_neighbors;
    }

    return neighbor_count;
}

void clear_neighbors(Site& site, ArrayX<int16_t>& neighbor_count, int min_neighbors) {
    if (neighbor_count[site.get_idx()] == 0)
        return;

    site.for_each_neighbour([&](Site neighbor, Hopping) {
        if (!neighbor.is_valid())
            return;

        auto const neighbor_idx = neighbor.get_idx();
        neighbor_count[neighbor_idx] -= 1;
        if (neighbor_count[neighbor_idx] < min_neighbors) {
            neighbor.set_valid(false);
            // recursive call... but it will not be very deep
            clear_neighbors(neighbor, neighbor_count, min_neighbors);
        }
    });

    neighbor_count[site.get_idx()] = 0;
}

ArrayX<sub_id> make_sublattice_ids(Foundation const& foundation) {
    ArrayX<sub_id> sublattice_ids(foundation.get_num_sites());
    for (auto const& site : foundation) {
        sublattice_ids[site.get_idx()] = static_cast<sub_id>(site.get_sublattice());
    }
    return sublattice_ids;
}

} // namespace detail

void remove_dangling(Foundation& foundation, int min_neighbors) {
    auto neighbor_count = detail::count_neighbors(foundation);
    for (auto& site : foundation) {
        if (!site.is_valid()) {
            detail::clear_neighbors(site, neighbor_count, min_neighbors);
        }
    }
}

Foundation::Foundation(Lattice const& lattice, Primitive const& primitive)
    : lattice(lattice),
      bounds(-primitive.size.array() / 2, (primitive.size.array() - 1) / 2),
      size(primitive.size),
      nsub(lattice.nsub()),
      num_sites(size.prod() * nsub),
      positions(detail::generate_positions(lattice.calc_position(bounds.first), size, lattice)),
      is_valid(ArrayX<bool>::Constant(num_sites, true)) {}

Foundation::Foundation(Lattice const& lattice, Shape const& shape)
    : lattice(lattice),
      bounds(detail::find_bounds(shape, lattice)),
      size((bounds.second - bounds.first) + Index3D::Ones()),
      nsub(lattice.nsub()),
      num_sites(size.prod() * nsub),
      positions(detail::generate_positions(lattice.calc_position(bounds.first), size, lattice)),
      is_valid(shape.contains(positions)) {
    remove_dangling(*this, lattice.get_min_neighbors());
}

HamiltonianIndices::HamiltonianIndices(Foundation const& foundation)
    : indices(ArrayX<int>::Constant(foundation.get_num_sites(), -1)), num_valid_sites(0) {
    // Assign Hamiltonian indices to all valid sites
    auto& is_valid = foundation.get_states();
    for (auto i = 0; i < foundation.get_num_sites(); ++i) {
        if (is_valid[i])
            indices[i] = num_valid_sites++;
    }
}

} // namespace cpb
