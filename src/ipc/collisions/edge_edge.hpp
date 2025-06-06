#pragma once

#include <ipc/candidates/edge_edge.hpp>
#include <ipc/collisions/collision.hpp>
#include <ipc/utils/eigen_ext.hpp>

namespace ipc {

class EdgeEdgeCollision : public EdgeEdgeCandidate, public Collision<4> {
public:
    EdgeEdgeCollision(
        const long edge0_id,
        const long edge1_id,
        const double eps_x,
        const EdgeEdgeDistanceType dtype = EdgeEdgeDistanceType::AUTO);

    EdgeEdgeCollision(
        const EdgeEdgeCandidate& candidate,
        const double eps_x,
        const EdgeEdgeDistanceType dtype = EdgeEdgeDistanceType::AUTO);

    EdgeEdgeCollision(
        const long edge0_id,
        const long edge1_id,
        const double eps_x,
        const double weight,
        const Eigen::SparseVector<double>& weight_gradient,
        const EdgeEdgeDistanceType dtype = EdgeEdgeDistanceType::AUTO);

    /// @brief Does the distance potentially have to be mollified?
    virtual bool is_mollified() const override { return true; }

    /// @brief Compute the mollifier threshold for the distance.
    /// @param rest_positions The stencil's rest vertex positions.
    /// @return The mollifier threshold.
    double
    mollifier_threshold(const VectorMax12d& rest_positions) const override;

    /// @brief Compute the mollifier for the distance.
    /// @param positions The stencil's vertex positions.
    /// @return The mollifier value.
    double mollifier(const VectorMax12d& positions) const override;

    /// @brief Compute the mollifier for the distance.
    /// @param positions The stencil's vertex positions.
    /// @param eps_x The mollifier's tolerance.
    /// @return The mollifier value.
    double
    mollifier(const VectorMax12d& positions, double eps_x) const override;

    /// @brief Compute the gradient of the mollifier for the distance w.r.t. positions.
    /// @param positions The stencil's vertex positions.
    /// @return The mollifier gradient.
    VectorMax12d
    mollifier_gradient(const VectorMax12d& positions) const override;

    /// @brief Compute the gradient of the mollifier for the distance wrt the positions.
    /// @param positions The stencil's vertex positions.
    /// @param eps_x The mollifier's tolerance.
    /// @return The mollifier gradient.
    VectorMax12d mollifier_gradient(
        const VectorMax12d& positions, double eps_x) const override;

    /// @brief Compute the Hessian of the mollifier for the distance w.r.t. positions.
    /// @param positions The stencil's vertex positions.
    /// @return The mollifier Hessian.
    MatrixMax12d
    mollifier_hessian(const VectorMax12d& positions) const override;

    /// @brief Compute the Hessian of the mollifier for the distance wrt the positions.
    /// @param positions The stencil's vertex positions.
    /// @param eps_x The mollifier's tolerance.
    /// @return The mollifier Hessian.
    MatrixMax12d mollifier_hessian(
        const VectorMax12d& positions, double eps_x) const override;

    /// @brief Compute the gradient of the mollifier for the distance w.r.t. rest positions.
    /// @param rest_positions The stencil's rest vertex positions.
    /// @param positions The stencil's vertex positions.
    /// @return The mollifier gradient w.r.t. rest positions.
    Vector12d mollifier_gradient_wrt_x(
        const VectorMax12d& rest_positions,
        const VectorMax12d& positions) const override;

    /// @brief Compute the jacobian of the distance mollifier's gradient w.r.t. rest positions.
    /// @param rest_positions The stencil's rest vertex positions.
    /// @param positions The stencil's vertex positions.
    /// @return The jacobian of the mollifier's gradient w.r.t. rest positions.
    Matrix12d mollifier_gradient_jacobian_wrt_x(
        const VectorMax12d& rest_positions,
        const VectorMax12d& positions) const override;

    // ------------------------------------------------------------------------

    EdgeEdgeDistanceType known_dtype() const override { return dtype; }

    // ------------------------------------------------------------------------

    bool operator==(const EdgeEdgeCollision& other) const;
    bool operator!=(const EdgeEdgeCollision& other) const;
    bool operator<(const EdgeEdgeCollision& other) const;

    template <typename H>
    friend H AbslHashValue(H h, const EdgeEdgeCollision& ee)
    {
        return H::combine(
            std::move(h), static_cast<const EdgeEdgeCandidate&>(ee), ee.dtype);
    }

    // ------------------------------------------------------------------------

    /// @brief Mollifier activation threshold.
    /// @see edge_edge_mollifier
    double eps_x;

    /// @brief Cached distance type.
    /// Some EE collisions are mollified EV or VV collisions.
    EdgeEdgeDistanceType dtype;
};

} // namespace ipc
