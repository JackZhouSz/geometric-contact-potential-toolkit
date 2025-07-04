#include <common.hpp>

#include <ipc/distance/point_plane.hpp>

using namespace ipc;

void define_point_plane_distance(py::module_& m)
{
    m.def(
        "point_plane_distance",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(&point_plane_distance),
        R"ipc_Qu8mg5v7(
        Compute the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            origin: The origin of the plane.
            normal: The normal of the plane.

        Returns:
            The distance between the point and plane.
        )ipc_Qu8mg5v7",
        "p"_a, "origin"_a, "normal"_a);

    m.def(
        "point_plane_distance",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(&point_plane_distance),
        R"ipc_Qu8mg5v7(
        Compute the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            t0: The first vertex of the triangle.
            t1: The second vertex of the triangle.
            t2: The third vertex of the triangle.

        Returns:
            The distance between the point and plane.
        )ipc_Qu8mg5v7",
        "p"_a, "t0"_a, "t1"_a, "t2"_a);

    m.def(
        "point_plane_distance_gradient",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(
            &point_plane_distance_gradient),
        R"ipc_Qu8mg5v7(
        Compute the gradient of the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            origin: The origin of the plane.
            normal: The normal of the plane.

        Returns:
            The gradient of the distance wrt p.
        )ipc_Qu8mg5v7",
        "p"_a, "origin"_a, "normal"_a);

    m.def(
        "point_plane_distance_gradient",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(
            &point_plane_distance_gradient),
        R"ipc_Qu8mg5v7(
        Compute the gradient of the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            t0: The first vertex of the triangle.
            t1: The second vertex of the triangle.
            t2: The third vertex of the triangle.

        Returns:
            The gradient of the distance wrt p, t0, t1, and t2.
        )ipc_Qu8mg5v7",
        "p"_a, "t0"_a, "t1"_a, "t2"_a);

    m.def(
        "point_plane_distance_hessian",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(
            &point_plane_distance_hessian),
        R"ipc_Qu8mg5v7(
        Compute the hessian of the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            origin: The origin of the plane.
            normal: The normal of the plane.

        Returns:
            The hessian of the distance wrt p.
        )ipc_Qu8mg5v7",
        "p"_a, "origin"_a, "normal"_a);

    m.def(
        "point_plane_distance_hessian",
        py::overload_cast<
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>,
            Eigen::ConstRef<const Eigen::Vector3d>>(
            &point_plane_distance_hessian),
        R"ipc_Qu8mg5v7(
        Compute the hessian of the distance between a point and a plane.

        Note:
            The distance is actually squared distance.

        Parameters:
            p: The point.
            t0: The first vertex of the triangle.
            t1: The second vertex of the triangle.
            t2: The third vertex of the triangle.

        Returns:
            The hessian of the distance wrt p, t0, t1, and t2.
        )ipc_Qu8mg5v7",
        "p"_a, "t0"_a, "t1"_a, "t2"_a);
}
