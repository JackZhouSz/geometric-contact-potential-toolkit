#include "point_edge.hpp"

#include <ipc/tangent/closest_point.hpp>

namespace ipc {
template <typename scalar, int dim>
scalar PointEdgeDistance<scalar, dim>::point_point_sqr_distance(
    const Eigen::Ref<const Vector<scalar, dim>>& a,
    const Eigen::Ref<const Vector<scalar, dim>>& b)
{
    return (a - b).squaredNorm();
}

template <typename scalar, int dim>
scalar PointEdgeDistance<scalar, dim>::point_line_sqr_distance(
    const Eigen::Ref<const Vector<scalar, dim>>& p,
    const Eigen::Ref<const Vector<scalar, dim>>& e0,
    const Eigen::Ref<const Vector<scalar, dim>>& e1)
{
    if constexpr (dim == 2)
        return Math<scalar>::sqr(Math<scalar>::cross2(e0 - p, e1 - p))
            / (e1 - e0).squaredNorm();
    else
        return (e0 - p).cross(e1 - p).squaredNorm() / (e1 - e0).squaredNorm();
}

template <typename scalar, int dim>
scalar PointEdgeDistance<scalar, dim>::point_edge_sqr_distance(
    const Eigen::Ref<const Vector<scalar, dim>>& p,
    const Eigen::Ref<const Vector<scalar, dim>>& e0,
    const Eigen::Ref<const Vector<scalar, dim>>& e1,
    const PointEdgeDistanceType dtype)
{
    switch (dtype) {
    case PointEdgeDistanceType::P_E:
        return point_line_sqr_distance(p, e0, e1);
    case PointEdgeDistanceType::P_E0:
        return point_point_sqr_distance(p, e0);
    case PointEdgeDistanceType::P_E1:
        return point_point_sqr_distance(p, e1);
    case PointEdgeDistanceType::AUTO:
    default:
        const Vector<scalar, dim> t = e1 - e0;
        const Vector<scalar, dim> pos = p - e0;
        const scalar s = pos.dot(t) / t.squaredNorm();
        return (pos - Math<scalar>::L_ns(s) * t).squaredNorm();
    }
}

template <typename scalar, int dim>
Vector<scalar, dim>
PointEdgeDistance<scalar, dim>::point_line_closest_point_direction(
    const Eigen::Ref<const Vector<scalar, dim>>& p,
    const Eigen::Ref<const Vector<scalar, dim>>& e0,
    const Eigen::Ref<const Vector<scalar, dim>>& e1)
{
    const Vector<scalar, dim> d = p - e0;
    const Vector<scalar, dim> t = e1 - e0;
    return d - (d.dot(t) / t.squaredNorm()) * t;
}

template <typename scalar, int dim>
Vector<scalar, dim>
PointEdgeDistance<scalar, dim>::point_edge_closest_point_direction(
    const Eigen::Ref<const Vector<scalar, dim>>& p,
    const Eigen::Ref<const Vector<scalar, dim>>& e0,
    const Eigen::Ref<const Vector<scalar, dim>>& e1,
    const PointEdgeDistanceType dtype)
{
    switch (dtype) {
    case PointEdgeDistanceType::P_E:
        return point_line_closest_point_direction(p, e0, e1);
    case PointEdgeDistanceType::P_E0:
        return p - e0;
    case PointEdgeDistanceType::P_E1:
        return p - e1;
    case PointEdgeDistanceType::AUTO:
    default:
        Vector<scalar, dim> t = e1 - e0;
        const Vector<scalar, dim> pos = p - e0;
        const scalar s = pos.dot(t) / t.squaredNorm();
        return pos - Math<scalar>::L_ns(s) * t;
    }
}

template <int dim>
std::tuple<Vector<double, dim>, Eigen::Matrix<double, dim, dim * dim>>
PointEdgeDistanceDerivatives<dim>::point_line_closest_point_direction_grad(
    const Eigen::Ref<const Vector<double, dim>>& p,
    const Eigen::Ref<const Vector<double, dim>>& e0,
    const Eigen::Ref<const Vector<double, dim>>& e1)
{
    Vector<double, dim> val;
    Eigen::Matrix<double, dim, dim * dim> grad;
#ifdef DERIVATIVES_WITH_AUTODIFF
    using T = ADGrad<dim * dim>;
    DiffScalarBase::setVariableCount(dim * dim);
    const Vector<T, dim> pT = slice_positions<T, 1, dim>(p);
    const Vector<T, dim> e0T = slice_positions<T, 1, dim>(e0, dim);
    const Vector<T, dim> e1T = slice_positions<T, 1, dim>(e1, 2 * dim);

    const Vector<T, dim> out =
        PointEdgeDistance<T, dim>::point_line_closest_point_direction(
            pT, e0T, e1T);
    for (int i = 0; i < dim; i++) {
        val(i) = out(i).getValue();
        grad.row(i) = out(i).getGradient();
    }
#else
    const double uv = point_edge_closest_point(p, e0, e1);
    const Vector<double, dim * dim> g =
        point_edge_closest_point_jacobian(p, e0, e1);

    val = (p - e0) - uv * (e1 - e0);

    grad.setZero();
    grad -= (e1 - e0) * g.transpose();
    grad.block(0, 0, dim, dim).diagonal().array() += 1.;
    grad.block(0, dim, dim, dim).diagonal().array() += -1. + uv;
    grad.block(0, 2 * dim, dim, dim).diagonal().array() -= uv;
#endif

    return std::make_tuple(val, grad);
}

template <int dim>
std::tuple<
    Vector<double, dim>,
    Eigen::Matrix<double, dim, dim * dim>,
    std::array<Eigen::Matrix<double, dim * dim, dim * dim>, dim>>
PointEdgeDistanceDerivatives<dim>::point_line_closest_point_direction_hessian(
    const Eigen::Ref<const Vector<double, dim>>& p,
    const Eigen::Ref<const Vector<double, dim>>& e0,
    const Eigen::Ref<const Vector<double, dim>>& e1)
{
    Vector<double, dim> val;
    Eigen::Matrix<double, dim, dim * dim> grad;
    std::array<Eigen::Matrix<double, dim * dim, dim * dim>, dim> hess;

#ifdef DERIVATIVES_WITH_AUTODIFF
    using T = ADHessian<dim * dim>;
    DiffScalarBase::setVariableCount(dim * dim);
    Vector<T, dim> pT = slice_positions<T, 1, dim>(p);
    Vector<T, dim> e0T = slice_positions<T, 1, dim>(e0, dim);
    Vector<T, dim> e1T = slice_positions<T, 1, dim>(e1, 2 * dim);

    Vector<T, dim> out =
        PointEdgeDistance<T, dim>::point_line_closest_point_direction(
            pT, e0T, e1T);
    for (int i = 0; i < dim; i++) {
        val(i) = out(i).getValue();
        grad.row(i) = out(i).getGradient();
        hess[i] = out(i).getHessian();
    }
#else
    const double uv = point_edge_closest_point(p, e0, e1);
    const Vector<double, dim * dim> g =
        point_edge_closest_point_jacobian(p, e0, e1);
    const Eigen::Matrix<double, dim * dim, dim * dim> h =
        point_edge_closest_point_hessian(p, e0, e1);

    val = (p - e0) - uv * (e1 - e0);

    grad.setZero();
    grad -= (e1 - e0) * g.transpose();
    grad.block(0, 0, dim, dim).diagonal().array() += 1.;
    grad.block(0, dim, dim, dim).diagonal().array() += -1. + uv;
    grad.block(0, 2 * dim, dim, dim).diagonal().array() -= uv;

    for (auto& hi : hess)
        hi.setZero();
    for (int d = 0; d < dim; d++) {
        // wrt. uv
        hess[d] -= (e1(d) - e0(d)) * h;

        // wrt. uv & e0
        hess[d].row(dim + d) += g.transpose();
        hess[d].col(dim + d) += g;

        // wrt. uv & e1
        hess[d].row(2 * dim + d) -= g.transpose();
        hess[d].col(2 * dim + d) -= g;
    }
#endif

    return std::make_tuple(val, grad, hess);
}

template <int dim>
std::tuple<Vector<double, dim>, Eigen::Matrix<double, dim, dim * dim>>
PointEdgeDistanceDerivatives<dim>::point_edge_closest_point_direction_grad(
    const Eigen::Ref<const Vector<double, dim>>& p,
    const Eigen::Ref<const Vector<double, dim>>& e0,
    const Eigen::Ref<const Vector<double, dim>>& e1,
    const PointEdgeDistanceType dtype)
{
    Vector<double, dim> vec;
    Eigen::Matrix<double, dim, dim * dim> grad =
        Eigen::Matrix<double, dim, dim * dim>::Zero();
#ifdef DERIVATIVES_WITH_AUTODIFF
    using T = ADGrad<dim * dim>;
    DiffScalarBase::setVariableCount(dim * dim);
    Vector<T, dim> pT = slice_positions<T, 1, dim>(p);
    Vector<T, dim> e0T = slice_positions<T, 1, dim>(e0, dim);
    Vector<T, dim> e1T = slice_positions<T, 1, dim>(e1, 2 * dim);

    Vector<T, dim> out =
        PointEdgeDistance<T, dim>::point_edge_closest_point_direction(
            pT, e0T, e1T, dtype);
    for (int i = 0; i < dim; i++) {
        vec(i) = out(i).getValue();
        grad.row(i) = out(i).getGradient();
    }
#else
    switch (dtype) {
    case PointEdgeDistanceType::P_E:
        std::tie(vec, grad) =
            point_line_closest_point_direction_grad(p, e0, e1);
        break;
    case PointEdgeDistanceType::P_E0:
        vec = p - e0;
        grad.leftCols(dim).diagonal().array() = 1.;
        grad.middleCols(3, dim).diagonal().array() = -1.;
        break;
    case PointEdgeDistanceType::P_E1:
        vec = p - e1;
        grad.leftCols(dim).diagonal().array() = 1.;
        grad.middleCols(6, dim).diagonal().array() = -1.;
        break;
    default:
        throw std::runtime_error("PointEdgeDistanceType not specified!");
    }
#endif

    return { vec, grad };
}

template <int dim>
std::tuple<
    Vector<double, dim>,
    Eigen::Matrix<double, dim, dim * dim>,
    std::array<Eigen::Matrix<double, dim * dim, dim * dim>, dim>>
PointEdgeDistanceDerivatives<dim>::point_edge_closest_point_direction_hessian(
    const Eigen::Ref<const Vector<double, dim>>& p,
    const Eigen::Ref<const Vector<double, dim>>& e0,
    const Eigen::Ref<const Vector<double, dim>>& e1,
    const PointEdgeDistanceType dtype)
{
    Vector<double, dim> vec;
    Eigen::Matrix<double, dim, dim * dim> grad =
        Eigen::Matrix<double, dim, dim * dim>::Zero();
    std::array<Eigen::Matrix<double, dim * dim, dim * dim>, dim> hess;
    for (auto& hi : hess)
        hi.setZero();
#ifdef DERIVATIVES_WITH_AUTODIFF
    using T = ADHessian<dim * dim>;
    DiffScalarBase::setVariableCount(dim * dim);
    Vector<T, dim> pT = slice_positions<T, 1, dim>(p);
    Vector<T, dim> e0T = slice_positions<T, 1, dim>(e0, dim);
    Vector<T, dim> e1T = slice_positions<T, 1, dim>(e1, 2 * dim);

    Vector<T, dim> out =
        PointEdgeDistance<T, dim>::point_edge_closest_point_direction(
            pT, e0T, e1T);

    for (int i = 0; i < dim; i++) {
        vec(i) = out(i).getValue();
        grad.row(i) = out(i).getGradient();
        hess[i] = out(i).getHessian();
    }
#else
    switch (dtype) {
    case PointEdgeDistanceType::P_E:
        std::tie(vec, grad, hess) =
            point_line_closest_point_direction_hessian(p, e0, e1);
        break;
    case PointEdgeDistanceType::P_E0:
        vec = p - e0;
        grad.leftCols(dim).diagonal().array() = 1.;
        grad.middleCols(3, dim).diagonal().array() = -1.;
        break;
    case PointEdgeDistanceType::P_E1:
        vec = p - e1;
        grad.leftCols(dim).diagonal().array() = 1.;
        grad.middleCols(6, dim).diagonal().array() = -1.;
        break;
    default:
        throw std::runtime_error("PointEdgeDistanceType not specified!");
    }
#endif

    return { vec, grad, hess };
}

template class PointEdgeDistance<double, 2>;
template class PointEdgeDistance<double, 3>;

template class PointEdgeDistance<ADGrad<4>, 2>;
template class PointEdgeDistance<ADHessian<4>, 2>;

template class PointEdgeDistance<ADGrad<6>, 2>;
template class PointEdgeDistance<ADHessian<6>, 2>;

template class PointEdgeDistance<ADGrad<10>, 2>;
template class PointEdgeDistance<ADHessian<10>, 2>;

template class PointEdgeDistance<ADGrad<9>, 3>;
template class PointEdgeDistance<ADHessian<9>, 3>;

template class PointEdgeDistance<ADGrad<12>, 3>;
template class PointEdgeDistance<ADHessian<12>, 3>;

template class PointEdgeDistance<ADGrad<13>, 3>;
template class PointEdgeDistance<ADHessian<13>, 3>;

#ifdef DERIVATIVES_WITH_AUTODIFF
template class PointEdgeDistance<ADGrad<15>, 3>;
template class PointEdgeDistance<ADHessian<15>, 3>;

template class PointEdgeDistance<ADGrad<18>, 3>;
template class PointEdgeDistance<ADHessian<18>, 3>;
#endif

template class PointEdgeDistanceDerivatives<2>;
template class PointEdgeDistanceDerivatives<3>;
} // namespace ipc
