#include "KPM.hpp"
#include "wrappers.hpp"
#include "thread.hpp"
using namespace cpb;

namespace {

template<template<class> class Strategy>
void wrap_kpm_strategy(py::module& m, char const* name) {
    auto const kpm_defaults = kpm::Config();
    m.def(
        name,
        [](Model const& model, std::pair<float, float> energy,
           kpm::Kernel const& kernel, int opt, float lanczos) {
            kpm::Config config;
            config.min_energy = energy.first;
            config.max_energy = energy.second;
            config.kernel = kernel;
            config.opt_level = opt;
            config.lanczos_precision = lanczos;

            return make_kpm<Strategy>(model, config);
        },
        "model"_a,
        "energy_range"_a=py::make_tuple(kpm_defaults.min_energy, kpm_defaults.max_energy),
        "kernel"_a=kpm_defaults.kernel,
        "optimization_level"_a=kpm_defaults.opt_level,
        "lanczos_precision"_a=kpm_defaults.lanczos_precision
    );
}

// This will be a lot simpler with C++14 generic lambdas
struct PyOptHam {
    using OptHamVariant = var::variant<kpm::OptimizedHamiltonian<float>,
                                       kpm::OptimizedHamiltonian<double>,
                                       kpm::OptimizedHamiltonian<std::complex<float>>,
                                       kpm::OptimizedHamiltonian<std::complex<double>>>;
    Hamiltonian h;
    OptHamVariant oh;

    struct MakeOH {
        int i;

        template<class scalar_t>
        OptHamVariant operator()(SparseMatrixRC<scalar_t> const& m) const {
            auto ret = kpm::OptimizedHamiltonian<scalar_t>(
                m.get(), {kpm::MatrixConfig::Reorder::ON, kpm::MatrixConfig::Format::CSR}
            );
            auto indices = std::vector<int>(m->rows());
            std::iota(indices.begin(), indices.end(), 0);
            auto bounds = kpm::Bounds<scalar_t>(m.get(), kpm::Config{}.lanczos_precision);
            ret.optimize_for({i, indices}, bounds.scaling_factors());
            return ret;
        }
    };

    struct ReturnMatrix {
        template<class scalar_t>
        ComplexCsrConstRef operator()(kpm::OptimizedHamiltonian<scalar_t> const& oh) const {
            return csrref(oh.csr());
        }
    };

    struct ReturnSizes {
        template<class scalar_t>
        std::vector<int> const& operator()(kpm::OptimizedHamiltonian<scalar_t> const& oh) const {
            return oh.sizes().get_data();
        }
    };

    struct ReturnIndices {
        template<class scalar_t>
        ArrayXi const& operator()(kpm::OptimizedHamiltonian<scalar_t> const& oh) const {
            return oh.idx().cols;
        }
    };
};

} // anonymously namespace

void wrap_greens(py::module& m) {
    py::class_<kpm::Stats>(m, "KPMStats")
        .def_readonly("num_moments", &kpm::Stats::num_moments)
        .def_readonly("num_operations", &kpm::Stats::num_operations)
        .def_readonly("matrix_memory", &kpm::Stats::matrix_memory)
        .def_readonly("vector_memory", &kpm::Stats::vector_memory)
        .def_property_readonly("ops", &kpm::Stats::ops)
        .def_property_readonly("elapsed_seconds", [](kpm::Stats const& s) {
            return s.moments_timer.elapsed_seconds();
        });

    py::class_<kpm::Kernel>(m, "KPMKernel");
    m.def("lorentz_kernel", &kpm::lorentz_kernel);
    m.def("jackson_kernel", &kpm::jackson_kernel);

    py::class_<KPM>(m, "Greens")
        .def("calc_greens", &KPM::calc_greens)
        .def("calc_greens", &KPM::calc_greens_vector)
        .def("calc_ldos", &KPM::calc_ldos)
        .def("deferred_ldos", [](py::object self, ArrayXd energy, double broadening,
                                 Cartesian position, std::string sublattice) {
            auto& kpm = self.cast<KPM&>();
            return Deferred<ArrayXd>{
                self, [=, &kpm] { return kpm.calc_ldos(energy, broadening, position, sublattice); }
            };
        })
        .def("report", &KPM::report, "shortform"_a=false)
        .def_property("model", &KPM::get_model, &KPM::set_model)
        .def_property_readonly("system", &KPM::system)
        .def_property_readonly("stats", &KPM::get_stats);

    wrap_kpm_strategy<kpm::DefaultStrategy>(m, "KPM");

#ifdef CPB_USE_CUDA
    wrap_kpm_strategy<kpm::CudaStrategy>(m, "KPMcuda");
#endif

    py::class_<PyOptHam>(m, "OptimizedHamiltonian")
        .def("__init__", [](PyOptHam& self, Hamiltonian const& h, int index) {
            new (&self) PyOptHam{h, var::apply_visitor(PyOptHam::MakeOH{index}, h.get_variant())};
        })
        .def_property_readonly("matrix", [](PyOptHam const& self) {
            return var::apply_visitor(PyOptHam::ReturnMatrix{}, self.oh);
        })
        .def_property_readonly("sizes", [](PyOptHam const& self) {
            return arrayref(var::apply_visitor(PyOptHam::ReturnSizes{}, self.oh));
        })
        .def_property_readonly("indices", [](PyOptHam const& self) {
            return arrayref(var::apply_visitor(PyOptHam::ReturnIndices{}, self.oh));
        });
}
