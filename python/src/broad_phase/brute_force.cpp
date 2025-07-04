#include <common.hpp>

#include <ipc/broad_phase/brute_force.hpp>

using namespace ipc;

void define_brute_force(py::module_& m)
{
    py::class_<BruteForce, BroadPhase, std::shared_ptr<BruteForce>>(
        m, "BruteForce")
        .def(py::init());
}
