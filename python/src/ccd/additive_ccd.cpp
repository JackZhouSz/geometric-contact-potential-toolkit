#include <common.hpp>

#include <ipc/ccd/additive_ccd.hpp>

using namespace ipc;

void define_additive_ccd(py::module_& m)
{
    py::class_<AdditiveCCD, NarrowPhaseCCD>(m, "AdditiveCCD")
        .def(
            py::init<const double, const double>(),
            R"ipc_Qu8mg5v7(
            Construct a new AdditiveCCD object.

            Parameters:
                conservative_rescaling: The conservative rescaling of the time of impact.
            )ipc_Qu8mg5v7",
            "max_iterations"_a = AdditiveCCD::DEFAULT_MAX_ITERATIONS,
            "conservative_rescaling"_a =
                AdditiveCCD::DEFAULT_CONSERVATIVE_RESCALING)
        .def_readonly_static(
            "DEFAULT_CONSERVATIVE_RESCALING",
            &AdditiveCCD::DEFAULT_CONSERVATIVE_RESCALING,
            "The default conservative rescaling value used to avoid taking steps exactly to impact. Value choosen to based on [Li et al. 2021].")
        .def_readwrite(
            "conservative_rescaling", &AdditiveCCD::conservative_rescaling,
            "The conservative rescaling value used to avoid taking steps exactly to impact.");
}
