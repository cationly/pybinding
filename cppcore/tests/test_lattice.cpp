#include <catch.hpp>

#include "Lattice.hpp"
using namespace cpb;

TEST_CASE("Sublattice") {
    auto sublattice = Sublattice{};
    sublattice.add_hopping({0, 0, 0}, 0, 0, false);
    REQUIRE_THROWS(sublattice.add_hopping({0, 0, 0}, 0, 0, false));
}

TEST_CASE("Lattice") {
    auto lattice = Lattice({1, 0, 0}, {0, 1, 0});
    REQUIRE(lattice.ndim() == 2);
    REQUIRE(lattice.get_vectors().capacity() == 2);
    REQUIRE(lattice.max_hoppings() == 0);

    SECTION("Add sublattices") {
        REQUIRE_THROWS(lattice.add_sublattice(""));

        lattice.add_sublattice("A");
        REQUIRE_FALSE(lattice.has_onsite_energy());
        REQUIRE_THROWS(lattice.add_sublattice("A"));

        lattice.add_sublattice("B", {0, 0, 0}, 1.0);
        REQUIRE(lattice.has_onsite_energy());

        lattice.add_sublattice("B2", {1, 0, 0}, 1.0, "B");
        REQUIRE_THROWS(lattice.add_sublattice("B3", {2, 0, 0}, 1.0, "bad_name"));

        while (lattice.nsub() != std::numeric_limits<sub_id>::max() + 1) {
            lattice.add_sublattice(std::to_string(lattice.nsub()));
        }
        REQUIRE_THROWS(lattice.add_sublattice("overflow"));
    }

    SECTION("Register hoppings") {
        REQUIRE_THROWS(lattice.register_hopping_energy("", 0.0));

        lattice.register_hopping_energy("t1", 1.0);
        REQUIRE_FALSE(lattice.has_complex_hoppings());
        REQUIRE_THROWS(lattice.register_hopping_energy("t1", 1.0));

        lattice.register_hopping_energy("t2", {0, 1.0});
        REQUIRE(lattice.has_complex_hoppings());

        while (lattice.get_hoppings().energy.size() != std::numeric_limits<hop_id>::max() + 1) {
            auto e = static_cast<double>(lattice.get_hoppings().energy.size());
            lattice.register_hopping_energy(std::to_string(e), e);
        }
        REQUIRE_THROWS(lattice.register_hopping_energy("overflow", 1.0));
    }

    SECTION("Add hoppings") {
        lattice.add_sublattice("A");
        lattice.add_sublattice("B");
        lattice.register_hopping_energy("t1", 1.0);

        REQUIRE_THROWS(lattice.add_registered_hopping({0, 0, 0}, "A", "A", "t1"));
        REQUIRE_THROWS(lattice.add_registered_hopping({0, 0, 0}, "bad_name", "A", "t1"));
        REQUIRE_THROWS(lattice.add_registered_hopping({0, 0, 0}, "A", "A", "bad_name"));

        lattice.add_registered_hopping({1, 0, 0}, "A", "A", "t1");
        REQUIRE_THROWS(lattice.add_registered_hopping({1, 0, 0}, "A", "A", "t1"));
        REQUIRE(lattice.max_hoppings() == 2);

        lattice.add_registered_hopping({1, 0, 0}, "A", "B", "t1");
        REQUIRE(lattice.max_hoppings() == 3);
        lattice.add_registered_hopping({1, 0, 0}, "B", "B", "t1");
        REQUIRE(lattice.max_hoppings() == 3);

        lattice.add_hopping({1, 1, 0}, "A", "A", 2.0);
        REQUIRE(lattice.get_hoppings().energy.size() == 2);
        lattice.add_hopping({1, 1, 0}, "A", "B", 2.0);
        REQUIRE(lattice.get_hoppings().energy.size() == 2);
    }

    SECTION("Calculate position") {
        lattice.add_sublattice("A", {0, 0, 0.5});
        REQUIRE(lattice.calc_position({1, 2, 0}, "A").isApprox(Cartesian(1, 2, 0.5)));
    }

    SECTION("Set offset") {
        REQUIRE_NOTHROW(lattice.set_offset({0.5f, 0.5f, 0}));
        REQUIRE_THROWS(lattice.set_offset({0.6f, 0, 0}));
        REQUIRE_THROWS(lattice.set_offset({0, -0.6f, 0}));

        auto const copy = lattice.with_offset({0.5f, 0, 0});
        REQUIRE(copy.calc_position({1, 2, 0}).isApprox(Cartesian(1.5f, 2, 0)));
    }

    SECTION("Min neighbors") {
        auto const copy = lattice.with_min_neighbors(3);
        REQUIRE(copy.get_min_neighbors() == 3);
    }
}

TEST_CASE("Lattice translate coordinates") {
    auto const lattice = Lattice({1, 0, 0}, {1, 1, 0});

    REQUIRE(lattice.translate_coordinates({1, 0, 0}).isApprox(Vector3f(1, 0, 0)));
    REQUIRE(lattice.translate_coordinates({1.5, 0.5, 0}).isApprox(Vector3f(1, 0.5, 0)));
    REQUIRE(lattice.translate_coordinates({0, 0, 1}).isApprox(Vector3f(0, 0, 0)));
}
