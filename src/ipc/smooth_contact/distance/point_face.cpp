#include "point_face.hpp"

#include "autogen.hpp"
#include "point_edge.hpp"

#include <ipc/tangent/closest_point.hpp>

namespace ipc {

std::tuple<Vector3d, Eigen::Matrix<double, 3, 12>>
point_plane_closest_point_direction_grad(
    const Eigen::Ref<const Vector3d>& p,
    const Eigen::Ref<const Vector3d>& t0,
    const Eigen::Ref<const Vector3d>& t1,
    const Eigen::Ref<const Vector3d>& t2)
{
    const Eigen::Vector2d uv = point_triangle_closest_point(p, t0, t1, t2);
    const Eigen::Matrix<double, 2, 12> jac =
        point_triangle_closest_point_jacobian(p, t0, t1, t2);
    const Vector3d direc =
        p - (t0 * (1 - uv(0) - uv(1)) + t1 * uv(0) + t2 * uv(1));
    Eigen::Matrix<double, 3, 12> grad =
        -(t1 - t0) * jac.row(0) - (t2 - t0) * jac.row(1);
    grad.middleCols<3>(3).diagonal().array() -= 1. - uv(0) - uv(1);
    grad.middleCols<3>(6).diagonal().array() -= uv(0);
    grad.middleCols<3>(9).diagonal().array() -= uv(1);
    grad.leftCols<3>().diagonal().array() += 1.;

    return { direc, grad };
}

std::tuple<Vector3d, Eigen::Matrix<double, 3, 12>, std::array<Matrix12d, 3>>
point_plane_closest_point_direction_hessian(
    const Eigen::Ref<const Vector3d>& p,
    const Eigen::Ref<const Vector3d>& t0,
    const Eigen::Ref<const Vector3d>& t1,
    const Eigen::Ref<const Vector3d>& t2)
{
    // compute derivatives of uv
    const Eigen::Vector2d uv = point_triangle_closest_point(p, t0, t1, t2);
    const Eigen::Matrix<double, 2, 12> jac =
        point_triangle_closest_point_jacobian(p, t0, t1, t2);
    std::array<Matrix12d, 2> H;
    autogen::triangle_closest_point_hessian_0(
        p(0), p(1), p(2), t0(0), t0(1), t0(2), t1(0), t1(1), t1(2), t2(0),
        t2(1), t2(2), H[0].data());
    autogen::triangle_closest_point_hessian_1(
        p(0), p(1), p(2), t0(0), t0(1), t0(2), t1(0), t1(1), t1(2), t2(0),
        t2(1), t2(2), H[1].data());

    // compute derivatives of the closest point
    const Vector3d direc =
        p - (t0 * (1 - uv(0) - uv(1)) + t1 * uv(0) + t2 * uv(1));

    Eigen::Matrix<double, 3, 12> grad =
        -(t1 - t0) * jac.row(0) - (t2 - t0) * jac.row(1);
    grad.middleCols<3>(3).diagonal().array() -= 1. - uv(0) - uv(1);
    grad.middleCols<3>(6).diagonal().array() -= uv(0);
    grad.middleCols<3>(9).diagonal().array() -= uv(1);
    grad.leftCols<3>().diagonal().array() += 1.;

    std::array<Matrix12d, 3> hess;
    for (auto& h : hess)
        h.setZero();
    for (int d = 0; d < 3; d++) {
        // wrt. uv
        hess[d] -= (t1(d) - t0(d)) * H[0] + (t2(d) - t0(d)) * H[1];

        // wrt. uv & t0
        hess[d].row(3 + d) += jac.row(0) + jac.row(1);
        hess[d].col(3 + d) += (jac.row(0) + jac.row(1)).transpose();

        // wrt. uv & t1
        hess[d].row(6 + d) -= jac.row(0);
        hess[d].col(6 + d) -= jac.row(0).transpose();

        // wrt. uv & t2
        hess[d].row(9 + d) -= jac.row(1);
        hess[d].col(9 + d) -= jac.row(1).transpose();
    }

    return { direc, grad, hess };
}

std::tuple<Vector3d, Eigen::Matrix<double, 3, 12>>
point_triangle_closest_point_direction_grad(
    const Eigen::Ref<const Vector3d>& p,
    const Eigen::Ref<const Vector3d>& t0,
    const Eigen::Ref<const Vector3d>& t1,
    const Eigen::Ref<const Vector3d>& t2,
    const PointTriangleDistanceType dtype)
{
    Vector3d pts = Vector3d::Zero();
    Eigen::Matrix<double, 3, 12> grad = Eigen::Matrix<double, 3, 12>::Zero();
    switch (dtype) {
    case PointTriangleDistanceType::P_T0:
        pts = p - t0;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(3) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_T1:
        pts = p - t1;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(6) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_T2:
        pts = p - t2;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(9) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_E0: {
        const auto [pts_, d_grad] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_grad(p, t0, t1);
        pts = pts_;
        grad(Eigen::all, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }) = d_grad;
        break;
    }

    case PointTriangleDistanceType::P_E1: {
        const auto [pts_, d_grad] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_grad(p, t1, t2);
        pts = pts_;
        grad(Eigen::all, { 0, 1, 2, 6, 7, 8, 9, 10, 11 }) = d_grad;
        break;
    }

    case PointTriangleDistanceType::P_E2: {
        const auto [pts_, d_grad] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_grad(p, t2, t0);
        pts = pts_;
        grad(Eigen::all, { 0, 1, 2, 9, 10, 11, 3, 4, 5 }) = d_grad;
        break;
    }

    case PointTriangleDistanceType::P_T:
        std::tie(pts, grad) =
            point_plane_closest_point_direction_grad(p, t0, t1, t2);
        break;

    default:
        throw std::invalid_argument(
            "Invalid distance type for point-triangle distance!");
    }

    return { pts, grad };
}

std::tuple<Vector3d, Eigen::Matrix<double, 3, 12>, std::array<Matrix12d, 3>>
point_triangle_closest_point_direction_hessian(
    const Eigen::Ref<const Vector3d>& p,
    const Eigen::Ref<const Vector3d>& t0,
    const Eigen::Ref<const Vector3d>& t1,
    const Eigen::Ref<const Vector3d>& t2,
    const PointTriangleDistanceType dtype)
{
    Vector3d pts = Vector3d::Zero();
    Eigen::Matrix<double, 3, 12> grad = Eigen::Matrix<double, 3, 12>::Zero();
    std::array<Matrix12d, 3> hess;
    for (auto& h : hess)
        h.setZero();
    switch (dtype) {
    case PointTriangleDistanceType::P_T0:
        pts = p - t0;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(3) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_T1:
        pts = p - t1;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(6) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_T2:
        pts = p - t2;
        grad.leftCols<3>() = Eigen::Matrix<double, 3, 3>::Identity();
        grad.middleCols<3>(9) = -Eigen::Matrix<double, 3, 3>::Identity();
        break;

    case PointTriangleDistanceType::P_E0: {
        const auto [d, d_grad, d_hess] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_hessian(p, t0, t1);
        pts = d;
        std::vector<int> ind { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        grad(Eigen::all, ind) = d_grad;
        for (int i = 0; i < 3; i++)
            hess[i](ind, ind) = d_hess[i];

        break;
    }

    case PointTriangleDistanceType::P_E1: {
        const auto [d, d_grad, d_hess] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_hessian(p, t1, t2);
        pts = d;
        std::vector<int> ind { 0, 1, 2, 6, 7, 8, 9, 10, 11 };
        grad(Eigen::all, ind) = d_grad;
        for (int i = 0; i < 3; i++)
            hess[i](ind, ind) = d_hess[i];

        break;
    }

    case PointTriangleDistanceType::P_E2: {
        const auto [d, d_grad, d_hess] = PointEdgeDistanceDerivatives<
            3>::point_line_closest_point_direction_hessian(p, t2, t0);
        pts = d;
        std::vector<int> ind { 0, 1, 2, 9, 10, 11, 3, 4, 5 };
        grad(Eigen::all, ind) = d_grad;
        for (int i = 0; i < 3; i++)
            hess[i](ind, ind) = d_hess[i];

        break;
    }

    case PointTriangleDistanceType::P_T:
        std::tie(pts, grad, hess) =
            point_plane_closest_point_direction_hessian(p, t0, t1, t2);
        break;

    default:
        throw std::invalid_argument(
            "Invalid distance type for point-triangle distance!");
    }

    return { pts, grad, hess };
}

} // namespace ipc